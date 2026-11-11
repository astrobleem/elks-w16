/*
 * MBTEST - Win16 MessageBox test program for the ELKS Win16 layer.
 *
 * Shows the four Windows 3.0 message box icons with different button sets, one box at a time,
 * so the icons, the buttons, the keyboard interface and the system menu (Close) can be checked.
 * The exit code is the sum of the four results (IDOK 1, IDCANCEL 2, IDYES 6, IDNO 7), which the
 * test compares with the buttons it chose; each result is also in w16's log.
 */
#include <windows.h>

int PASCAL WinMain(HANDLE hInstance, HANDLE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    int r;

    (void)hInstance; (void)lpszCmdLine; (void)nCmdShow;
    if (hPrevInstance)
        return 0;
    r = MessageBox(NULL, "A stop sign: MB_ICONHAND.", "Hand", MB_OK | MB_ICONHAND);
    r += MessageBox(NULL, "A question: MB_ICONQUESTION. Yes or no?", "Question", MB_YESNO | MB_ICONQUESTION);
    r += MessageBox(NULL, "A warning: MB_ICONEXCLAMATION.", "Exclamation", MB_OKCANCEL | MB_ICONEXCLAMATION);
    r += MessageBox(NULL, "Information: MB_ICONASTERISK.", "Asterisk", MB_OK | MB_ICONASTERISK);
    return r;
}
