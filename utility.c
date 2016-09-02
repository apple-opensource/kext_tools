/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include "utility.h"
#include <sys/resource.h>

/*******************************************************************************
* createCFString()
*******************************************************************************/
CFStringRef createCFString(char * string)
{
    return CFStringCreateWithCString(kCFAllocatorDefault, string,
        kCFStringEncodingUTF8);
}


/*******************************************************************************
* check_file()
*
* This function makes sure that a given file exists, is a regular file, and
* is readable.
*******************************************************************************/
Boolean check_file(const char * filename)
{
    Boolean result = true;  // assume success
    struct stat stat_buf;

    if (stat(filename, &stat_buf) != 0) {
        perror(filename);
        result = false;
        goto finish;
    }

    if ( !(stat_buf.st_mode & S_IFREG) ) {
        qerror("%s is not a regular file\n", filename);
        result = false;
        goto finish;
    }

    if (access(filename, R_OK) != 0) {
        qerror("%s is not readable\n", filename);
        result = false;
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
* check_dir()
*
* This function makes sure that a given directory exists, and is writeable.
*******************************************************************************/
Boolean check_dir(const char * dirname, int writeable, int print_err)
{
    int result = true;  // assume success
    struct stat stat_buf;

    if (stat(dirname, &stat_buf) != 0) {
        if (print_err) {
            perror(dirname);
        }
        result = false;
        goto finish;
    }

    if ( !(stat_buf.st_mode & S_IFDIR) ) {
// XXX This could be called on a kext, message should say such
        if (print_err) {
            qerror("%s is not a directory\n", dirname);
        }
        result = false;
        goto finish;
    }

    if (writeable) {
        if (access(dirname, W_OK) != 0) {
            if (print_err) {
                qerror("%s is not writeable\n", dirname);
            }
            result = false;
            goto finish;
        }
    }
finish:
    return result;
}

/*******************************************************************************
* qerror()
*
* Quick wrapper over printing that checks verbose level. Does not append a
* newline like error_log() does.
*******************************************************************************/
void qerror(const char * format, ...)
{
    va_list ap;

    if (g_verbose_level <= kKXKextManagerLogLevelSilent) return;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fflush(stderr);

    return;
}

/*******************************************************************************
* verbose_log()
*
* Print a log message prefixed with the name of the program.
*******************************************************************************/
void verbose_log(const char * format, ...)
{
    va_list ap;

    if (g_verbose_level < kKXKextManagerLogLevelDefault) return;

    fprintf(stdout, "%s: ", progname);

    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fprintf(stdout, "\n");

    fflush(stdout);

    return;
}

/*******************************************************************************
* error_log()
*
* Print an error message prefixed with the name of the program.
*******************************************************************************/
void error_log(const char * format, ...)
{
    va_list ap;

    if (g_verbose_level <= kKXKextManagerLogLevelSilent) return;

    fprintf(stderr, "%s: ", progname);

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    fflush(stderr);

    return;
}

/*******************************************************************************
* user_approve()
*
* Ask the user a question and wait for a yes/no answer.
*******************************************************************************/
int user_approve(int default_answer, const char * format, ...)
{
    int result = 1;
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string;
    char * prompt_string = NULL;
    int c, x;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("memory allocation failure\n");
        result = -1;
        goto finish;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    prompt_string = default_answer ? " [Y/n]" : " [y/N]";
    
    while ( 1 ) {
        fprintf(stdout, "%s%s%s", output_string, prompt_string, "? ");
        fflush(stdout);

        c = fgetc(stdin);

        if (c == EOF) {
            result = -1;
            goto finish;
        }

       /* Make sure we get a newline.
        */
        if ( c != '\n' ) {
            do {
                x = fgetc(stdin);
            } while (x != '\n' && x != EOF);

            if (x == EOF) {
                result = -1;
                goto finish;
            }
        }

        if (c == '\n') {
            result = default_answer ? 1 : 0;
            goto finish;
        } else if (tolower(c) == 'y') {
            result = 1;
            goto finish;
        } else if (tolower(c) == 'n') {
            result = 0;
            goto finish;
        }
    }

finish:
    if (output_string) free(output_string);

    return result;
}

/*******************************************************************************
* user_input()
*
* Ask the user for input.
*******************************************************************************/
const char * user_input(const char * format, ...)
{
    char * result = NULL;  // return value
    va_list ap;
    char fake_buffer[2];
    int output_length;
    char * output_string = NULL;
    unsigned index;
    size_t size = 80;  // more than enough to input a hex address
    int c;

    result = (char *)malloc(size);
    if (!result) {
        goto finish;
    }
    index = 0;

    va_start(ap, format);
    output_length = vsnprintf(fake_buffer, 1, format, ap);
    va_end(ap);

    output_string = (char *)malloc(output_length + 1);
    if (!output_string) {
        qerror("memory allocation failure\n");
        result = NULL;
        goto finish;
    }

    va_start(ap, format);
    vsprintf(output_string, format, ap);
    va_end(ap);

    fprintf(stdout, "%s ", output_string);
    fflush(stdout);

    c = fgetc(stdin);
    while (c != '\n' && c != EOF) {
        if (index >= (size - 1)) {
            qerror("input line too long\n");
            if (result) free(result);
            result = NULL;
            goto finish;
        }
        result[index++] = (char)c;
        c = fgetc(stdin);
    }

    result[index] = '\0';

    if (c == EOF) {
        if (result) free(result);
        result = NULL;
        goto finish;
    }

finish:
    if (output_string) free(output_string);

    return result;
}

/*******************************************************************************
* addKextsToManager()
*
* Add the kexts named in the kextNames array to the given kext manager, and
* put their names into the kextNamesToUse.
*
* Return values:
*   1: all kexts added successfully
*   0: one or more could not be added
*  -1: program-fatal error; exit as soon as possible
*******************************************************************************/
int addKextsToManager(
    KXKextManagerRef aManager,
    CFArrayRef kextNames,
    CFMutableArrayRef kextNamesToUse,
    Boolean do_tests)
{
    int result = 1;     // assume success
    KXKextManagerError kxresult = kKXKextManagerErrorNone;
    CFIndex i, count;
    KXKextRef theKext = NULL;  // don't release
    CFURLRef kextURL = NULL;   // must release

   /*****
    * Add each kext named to the manager.
    */
    count = CFArrayGetCount(kextNames);
    for (i = 0; i < count; i++) {
        char name_buffer[MAXPATHLEN];

        CFStringRef kextName = (CFStringRef)CFArrayGetValueAtIndex(
            kextNames, i);

        if (kextURL) {
            CFRelease(kextURL);
            kextURL = NULL;
        }

        kextURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
            kextName, kCFURLPOSIXPathStyle, true);
        if (!kextURL) {
            qerror("memory allocation failure\n");
            result = -1;
            goto finish;
        }

        if (!CFStringGetCString(kextName,
            name_buffer, sizeof(name_buffer) - 1, kCFStringEncodingUTF8)) {

            qerror("memory allocation or string conversion error\n");
            result = -1;
            goto finish;
        }

        kxresult = KXKextManagerAddKextWithURL(aManager, kextURL, true, &theKext);
        if (kxresult != kKXKextManagerErrorNone) {
            result = 0;
            qerror("can't add kernel extension %s (%s)",
                name_buffer, KXKextManagerErrorStaticCStringForError(kxresult));
            qerror(" (run %s on this kext with -t for diagnostic output)\n",
                progname);
#if 0
            if (do_tests && theKext && g_verbose_level >= kKXKextManagerLogLevelErrorsOnly) {
                qerror("kernel extension problems:\n");
                KXKextPrintDiagnostics(theKext, stderr);
            }
            continue;
#endif 0
        }
        if (kextNamesToUse && theKext &&
            (kxresult == kKXKextManagerErrorNone || do_tests)) {

            CFArrayAppendValue(kextNamesToUse, kextName);
        }
    }

finish:
    if (kextURL) CFRelease(kextURL);
    return result;
}

/*******************************************************************************
* Fork a process after a specified delay, and either wait on it to exit or
* leave it to run in the background.
*
* Returns -2 on fork() failure, -1 on other failure, and depending on wait:
* wait:true - exit status of forked program
* wait: false - pid of background process
*******************************************************************************/
int fork_program(const char * argv0, char * const argv[], int delay, Boolean wait)
{
    int result = -2;
    int status;
    pid_t pid;

    switch (pid = fork()) {
        case -1:  // error
            goto finish;
            break;

        case 0:  // child
            if (!wait) {
                // child forks for async/no zombies
                result = daemon(0, 0);
                if (result == -1) {
                    goto finish;
                }
                // XX does this policy survive the exec below?
                setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_THROTTLE);
            }

            if (delay) {
                sleep(delay);
            }

            execv(argv0, argv);
            // if execv returns, we have an error but no clear way to log it
            exit(1);

        default:  // parent
            waitpid(pid, &status, 0);
            status = WEXITSTATUS(status);
            if (wait) {
                result = status;
            } else if (status != 0) {
                result = -1;
            } else {
                result = pid;
            }
            break;
    }

finish:
    return result;
}

