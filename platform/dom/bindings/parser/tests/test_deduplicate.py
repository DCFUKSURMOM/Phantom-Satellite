import WebIDL

def WebIDLTest(parser, harness):
    parser.parse("""
        interface Foo;
        interface Bar;
        interface Foo;
        """);

    results = parser.finish()

    # There should be no duplicate interfaces in the result.
    expectedNames = sorted(['Foo', 'Bar'])
    actualNames = sorted([iface.identifier.name for iface in results])
    harness.check(actualNames, expectedNames, "Parser shouldn't output duplicate names.")
