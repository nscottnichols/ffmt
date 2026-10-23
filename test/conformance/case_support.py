"""Small constructors for trusted, handwritten conformance fixtures."""
def case(name, family, pattern, *args, **options):
    return dict(id=name, family=family, pattern=pattern, args=list(args), **options)
