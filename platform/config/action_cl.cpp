/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * C++ replacement for mozbuild/action/cl.py that wraps cl.exe on Windows.
 * This is a Windows-only host tool, built well before XPCOM.
 */
#include <windows.h>
#include <string> // We don't use XPCOM string classes here.
#include <vector>
#include <fstream>
#include <algorithm>
#pragma warning(disable:4530)
using namespace std;


/* Make passed argv into a command line for cl.exe */
static wstring quoteArg(const wstring& aCmdArgv) {
  bool addQuotes = aCmdArgv.find_first_of(L" \t\"") != wstring::npos;

  if (!addQuotes) {
    return aCmdArgv;
  }

  wstring out = L"\"";
  size_t backslash = 0;

  for (wchar_t ch : aCmdArgv) {
    if (ch == L'\\') {
      backslash++;
    } else if (ch == L'"') {
      // Escape all backslashes before a quote
      out.append(backslash * 2, L'\\');
      backslash = 0;
      out += L"\\\"";
    } else {
      out.append(backslash, L'\\');
      backslash = 0;
      out += ch;
    }
  }

  out.append(backslash * 2, L'\\');
  out += L"\"";

  return out;
}

/* We need to detect the showIncludes prefix to extract raw paths for
   dep files, and drop the lines from display to avoid console spam. */
static string showIncludesPrefixDetection(const string& aLine) {
  // Look for absolute Windows paths
  size_t pos = string::npos;

  // Drive letter: C:\...
  for (size_t i = 0; i + 2 < aLine.size(); i++) {
    if (isalpha((unsigned char)aLine[i]) &&
        aLine[i + 1] == ':' &&
        (aLine[i + 2] == '\\' || aLine[i + 2] == '/')) {
      pos = i;
      break;
    }
  }

  // UNC path: \\server\share
  if (pos == string::npos) {
    if (aLine.size() >= 2 && aLine[0] == '\\' && aLine[1] == '\\') {
      pos = 0;
    }
  }

  if (pos == string::npos) {
    return "";
  }

  // Prefix is everything before the path
  return aLine.substr(0, pos);
}

/* Extract the header path from an include line using the known prefix */
static string extractHeaderPath(const string& aLine, const string& aPrefix) {
  if (aLine.rfind(aPrefix, 0) != 0) {
    return "";
  }

  // Strip prefix
  string path = aLine.substr(aPrefix.size());

  // Trim leading whitespace
  while (!path.empty() && (path[0] == ' ' || path[0] == '\t')) {
    path.erase(0, 1);
  }

  // Remove trailing newline
  if (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
    path.pop_back();
  }

  return path;
}


/* Sanitize a dependency path for Makefile depfiles.
   Performs:
   1. Strip trailing CR/LF
   2. Strip leading MSVC indentation
   3. Normalize slashes to '/'
   4. Escape spaces and backslashes for Make */
static string sanitizeMakeDeps(string aLine) {
  // 1. Strip trailing CR/LF
  while (!aLine.empty() && (aLine.back() == '\r' || aLine.back() == '\n')) {
    aLine.pop_back();
  }

  // 2. Strip leading indentation (MSVC indents include lines)
  while (!aLine.empty() && (aLine.front() == ' ' || aLine.front() == '\t')) {
    aLine.erase(aLine.begin());
  }

  // 3. Normalize slashes
  for (char& ch : aLine) {
    if (ch == '\\') {
      ch = '/';
    }
  }

  // 4. Escape for Make
  string out;
  out.reserve(aLine.size() * 2);

  for (char ch : aLine) {
    if (ch == ' ') {
      out += "\\ ";
    } else if (ch == '\\') {
      // After normalization this shouldn't occur, but safe anyway
      out += "\\\\";
    } else {
      out += ch;
    }
  }

  return out;
}

/* MSYS1 does not use wide APIs, so ensure we get the narrow version here. */
static string getWinEnvVar(const char* aName) {
  char buffer[32768];
  DWORD length = GetEnvironmentVariableA(aName, buffer, sizeof(buffer));

  if (length == 0 || length >= sizeof(buffer)) {
    return string();
  }

  return string(buffer, length);
}

/* Exclude system headers by ignoring anything not in our directories.
 * We don't know where people will put their objdir, so it has to be
 * tracked separately.
 */
static bool isProjectDep(const string& aPath,
                         const string& aTopSrcDir,
                         const string& aTopObjDir) {
  // We need forward slashes for our build system.
  auto norm = [](string aStr) {
    replace(aStr.begin(), aStr.end(), '\\', '/');
    return aStr;
  };

  string path = norm(aPath);
  string topSrcDir = norm(aTopSrcDir);
  string topObjDir = norm(aTopObjDir);

  // Must be absolute or relative under one of the two roots
  return (path.rfind(topSrcDir, 0) == 0) ||
         (path.rfind(topObjDir, 0) == 0);
}

/* Read stdout from the compiler pipe line-by-line, filtering include
   lines and collecting dependency paths. */
static vector<string> processOutput(HANDLE aReadPipe) {
  string lineBuf;
  vector<string> deps;
  string topSrcDir = getWinEnvVar("CL_TOPSRCDIR");
  string topObjDir = getWinEnvVar("CL_TOPOBJDIR");
  char buffer[4096];
  DWORD bytesRead;

  string includePrefix;
  bool prefixFound = false;

  while (ReadFile(aReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr)) {
    lineBuf.append(buffer, bytesRead);

    size_t pos;
    while ((pos = lineBuf.find('\n')) != string::npos) {
      string line = lineBuf.substr(0, pos + 1);
      lineBuf.erase(0, pos + 1);

      if (!prefixFound) {
        string maybe = showIncludesPrefixDetection(line);
        if (!maybe.empty()) {
          includePrefix = maybe;
          prefixFound = true;
        }
      }

      // If prefix is known and this line starts with it, it's an include line
      if (prefixFound && line.rfind(includePrefix, 0) == 0) {
        string header = extractHeaderPath(line, includePrefix);
        if (!header.empty()) {
          header = sanitizeMakeDeps(header);
          if (isProjectDep(header, topSrcDir, topObjDir)) {
            deps.push_back(header);
          }
        }
        // This is an include line, don't print it
        continue;
      }

      // Otherwise, just print
      fwrite(line.data(), 1, line.size(), stdout);
    }
  }

  if (!lineBuf.empty()) {
    fwrite(lineBuf.data(), 1, lineBuf.size(), stdout);
  }

  return deps;
}

/* Convert a UTF-16 wide string to a UTF-8 narrow string. */
static string wideToUtf8(const wstring& aWide) {
  if (aWide.empty()) {
    return {};
  }

  int len = WideCharToMultiByte(CP_UTF8, 0, aWide.c_str(), -1, nullptr, 0, nullptr, nullptr);

  string out(len - 1, '\0');

  WideCharToMultiByte(CP_UTF8, 0, aWide.c_str(), -1, &out[0], len - 1, nullptr, nullptr);

  return out;
}

/* Compute the .pp depfile path next to the given object file. */
static string makeDepfilePath(const string& aObjUtf8) {
  size_t pos = aObjUtf8.find_last_of("/\\");
  string dir = (pos == string::npos) ? "" : aObjUtf8.substr(0, pos + 1);
  string base = (pos == string::npos) ? aObjUtf8 : aObjUtf8.substr(pos + 1);

  return dir + ".deps/" + base + ".pp";
}

/* Write a Make-style dependency file for the given object and dep list. */
static void writeDepfile(const wstring& aObjFile, const vector<string>& aDeps) {
  string objUtf8 = wideToUtf8(aObjFile);
  string ppName = makeDepfilePath(objUtf8);

  ofstream out(ppName);
  out << objUtf8 << ":";

  for (auto& dep : aDeps) {
    out << " " << dep;
  }

  out << "\r\n";
}

int wmain(int argc, wchar_t** argv) {
  if (argc < 2) {
    fwprintf(stderr, L"Usage: action_cl.exe <compiler> [args...]\n");
    return 1;
  }

  const wchar_t* compiler = argv[1];

  // Build command line
  wstring cmd = quoteArg(compiler);
  wstring objFile;
  wstring sourceFile;
  cmd += L" -showIncludes";

  for (int i = 2; i < argc; i++) {
    cmd += L" ";
    cmd += quoteArg(argv[i]);
  }

  vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
  cmdLine.push_back(L'\0');

  for (int i = 2; i < argc; i++) {
    wstring arg = argv[i];

    if (arg.rfind(L"/Fo", 0) == 0 || arg.rfind(L"-Fo", 0) == 0) {
      objFile = arg.substr(3); // strip /Fo or -Fo
    }

    if (!arg.empty() && arg[0] != L'-' && arg[0] != L'/') {
      sourceFile = arg; // last non-flag argument is the source file
    }
  }

  // Create pipe for capturing stdout
  HANDLE readPipe, writePipe;
  SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
    fwprintf(stderr, L"Failed to create pipe\n");
    return 1;
  }

  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si = { sizeof(si) };
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = writePipe;
  si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
  si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi;

  BOOL ok = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);

  CloseHandle(writePipe);

  if (!ok) {
    fwprintf(stderr, L"Failed to launch compiler: %ls (error %lu)\n", compiler, GetLastError());
    CloseHandle(readPipe);
    return 1;
  }

  // Read compiler output and collect dependencies
  auto deps = processOutput(readPipe);
  CloseHandle(readPipe);

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = 0;
  GetExitCodeProcess(pi.hProcess, &exitCode);

  // Write the depfile only if we know the output object path
  if (!objFile.empty()) {
    string srcUtf8 = wideToUtf8(sourceFile);
    deps.insert(deps.begin(), sanitizeMakeDeps(srcUtf8));
    writeDepfile(objFile, deps);
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return exitCode;
}
