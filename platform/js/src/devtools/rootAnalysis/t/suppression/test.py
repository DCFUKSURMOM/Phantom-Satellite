test.compile("source.cpp")
test.run_analysis_script('gcTypes', upto='gcFunctions')

# The suppressions file uses only mangled names since it's for internal use,
# though I may change that soon given (1) the unfortunate non-uniqueness of
# mangled constructor names, and (2) the usefulness of this file for
# mrgiggles's reporting.
suppressed = test.load_suppressed_functions()

# Only one of these is fully suppressed (ie, *always* called within the scope
# of an AutoSuppressGC).
assert(len([f for f in suppressed if 'suppressedFunction' in f]) == 1)
assert(len([f for f in suppressed if 'halfSuppressedFunction' in f]) == 0)
assert(len([f for f in suppressed if 'unsuppressedFunction' in f]) == 0)

# gcFunctions should be the inverse, but we get to rely on unmangled names here.
gcFunctions = test.load_gcFunctions()
print(gcFunctions)
assert('void GC()' in gcFunctions)
assert('void suppressedFunction()' not in gcFunctions)
assert('void halfSuppressedFunction()' in gcFunctions)
assert('void unsuppressedFunction()' in gcFunctions)
assert('void f()' in gcFunctions)
