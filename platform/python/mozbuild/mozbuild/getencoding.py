def getencoding():
    import sys
    import ctypes
    from locale import getpreferredencoding

    if sys.platform.startswith("win"):
       try:
          codepage = ctypes.windll.kernel32.GetConsoleOutputCP()
          if codepage:
              encoding = "cp%d" % codepage
          else:
              encoding = getpreferredencoding(False)
       except Exception:
          encoding = getpreferredencoding(False)
    else:
       encoding = getpreferredencoding(False)

    return encoding
