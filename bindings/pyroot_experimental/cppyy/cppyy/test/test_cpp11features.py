import py, os, sys
from pytest import raises
from .support import setup_make

try:
    import __pypy__
    is_pypy = True
except ImportError:
    is_pypy = False


currpath = py.path.local(__file__).dirpath()
test_dct = str(currpath.join("cpp11featuresDict"))

def setup_module(mod):
    setup_make("cpp11features")

class TestCPP11FEATURES:
    def setup_class(cls):
        cls.test_dct = test_dct
        import cppyy
        cls.cpp11features = cppyy.load_reflection_info(cls.test_dct)

    def test01_shared_ptr(self):
        """Usage and access of std::shared_ptr<>"""

        from cppyy.gbl import TestSharedPtr, create_shared_ptr_instance

      # proper memory accounting
        assert TestSharedPtr.s_counter == 0

        ptr1 = create_shared_ptr_instance()
        assert ptr1
        assert not not ptr1
        assert TestSharedPtr.s_counter == 1

        ptr2 = create_shared_ptr_instance()
        assert ptr2
        assert not not ptr2
        assert TestSharedPtr.s_counter == 2

        del ptr2
        import gc; gc.collect()
        assert TestSharedPtr.s_counter == 1

        del ptr1
        gc.collect()
        assert TestSharedPtr.s_counter == 0

    def test02_shared_ptr_construction(self):
        """Shared pointer ctor is templated, taking special care"""

        from cppyy.gbl import std, TestSharedPtr

      # proper memory accounting
        assert TestSharedPtr.s_counter == 0

        class C(TestSharedPtr):
            pass

        c = C()
        assert TestSharedPtr.s_counter == 1
        c.__python_owns__ = False
        cc = std.shared_ptr[TestSharedPtr](c)

        del c

        assert cc
        assert TestSharedPtr.s_counter == 1

        del cc

        import gc
        gc.collect()
        assert TestSharedPtr.s_counter == 0

    def test03_shared_ptr_memory_handling(self):
        """Test shared pointer memory ownership"""

        from cppyy.gbl import std, TestSharedPtr

      # proper memory accounting
        assert TestSharedPtr.s_counter == 0

        t = TestSharedPtr()
        assert TestSharedPtr.s_counter == 1
        assert t.__python_owns__

        tt = std.shared_ptr[TestSharedPtr](t)
        assert not t.__python_owns__

        class C(TestSharedPtr):
            pass

        c = C()
        assert TestSharedPtr.s_counter == 2
        assert c.__python_owns__

        cc = std.shared_ptr[TestSharedPtr](c)
        assert not c.__python_owns__

        del cc, tt

        import gc
        gc.collect()
        assert TestSharedPtr.s_counter == 0

    def test04_shared_ptr_passing(self):
        """Ability to pass shared_ptr<Derived> through shared_ptr<Base>"""

        from cppyy.gbl import std, TestSharedPtr, DerivedTestSharedPtr
        from cppyy.gbl import pass_shared_ptr

    # proper memory accounting
        assert TestSharedPtr.s_counter == 0

        dd = std.make_shared[DerivedTestSharedPtr](DerivedTestSharedPtr(24))
        assert TestSharedPtr.s_counter == 1

        assert pass_shared_ptr(dd) == 100

        del dd

        import gc
        gc.collect()
        assert TestSharedPtr.s_counter == 0

    def test05_nullptr(self):
        """Allow the programmer to pass NULL in certain cases"""
      
        import cppyy

      # test existence
        nullptr = cppyy.nullptr
      # assert not hasattr(cppyy.gbl, 'nullptr')

      # usage is tested in datatypes.py:test15_nullptr_passing
 
    def test06_move(self):
        """Move construction, assignment, and methods"""

        import cppyy

        def moveit(T):
            from cppyy.gbl import std

          # move constructor
            i1 = T()
            assert T.s_move_counter == 0

            i2 = T(i1)  # cctor
            assert T.s_move_counter == 0

            if is_pypy or 0x3000000 <= sys.hexversion:
                i3 = T(std.move(T()))            # can't check ref-count
            else:
                i3 = T(T()) # should call move, not memoized cctor
            assert T.s_move_counter == 1

            i3 = T(std.move(T()))                # both move and ref-count
            assert T.s_move_counter == 2

            i4 = T(std.move(i1))
            assert T.s_move_counter == 3

          # move assignment
            i4.__assign__(i2)
            assert T.s_move_counter == 3

            if is_pypy or 0x3000000 <= sys.hexversion:
                i4.__assign__(std.move(T()))     # can't check ref-count
            else:
                i4.__assign__(T())
            assert T.s_move_counter == 4

            i4.__assign__(std.move(i2))
            assert T.s_move_counter == 5

      # order of moving and normal functions are reversed in 1, 2, for
      # overload resolution testing
        moveit(cppyy.gbl.TestMoving1)
        moveit(cppyy.gbl.TestMoving2)

    def test07_initializer_list(self):
        """Initializer list construction"""

        from cppyy.gbl import std, TestData, TestData2, WithInitList

        v = std.vector[int]((1, 2, 3, 4))
        assert list(v) == [1, 2, 3, 4]

        v = std.vector['double']((1, 2, 3, 4))
        assert list(v) == [1., 2., 3., 4.]

        raises(TypeError, std.vector[int], [1., 2., 3., 4.])

        for cls in [std.vector, WithInitList]:
            for cls_arg in [TestData, TestData2]:
                l = list()
                for i in range(10):
                    l.append(cls_arg(i))

                v = cls[cls_arg](l)
                assert len(v) == len(l)
                for i in range(len(l)):
                    assert v[i].m_int == l[i].m_int

    def test08_lambda_calls(self):
        """Call (global) lambdas"""

        import cppyy

        cppyy.cppdef("auto gMyLambda = [](int a) { return 40 + a; };")

        assert cppyy.gbl.gMyLambda
        assert cppyy.gbl.gMyLambda(2)  == 42
        assert cppyy.gbl.gMyLambda(40) == 80

        if cppyy.gbl.gInterpreter.ProcessLine("__cplusplus;") >= 201402:
            cppyy.cppdef("auto gime_a_lambda1() { return []() { return 42; }; }")
            l1 = cppyy.gbl.gime_a_lambda1()
            assert l1
            assert l1() == 42

            cppyy.cppdef("auto gime_a_lambda2() { int a = 4; return [a](int b) { return 42+a+b; }; }")
            l2 = cppyy.gbl.gime_a_lambda2()
            assert l2
            assert l2(2) == 48

            cppyy.cppdef("auto gime_a_lambda3(int a ) { return [a](int b) { return 42+a+b; }; }")
            l3 = cppyy.gbl.gime_a_lambda3(4)
            assert l3
            assert l3(2) == 48

    def test09_optional(self):
        """Use of optional and nullopt"""

        import cppyy

        if 201703 <= cppyy.gbl.gInterpreter.ProcessLine("__cplusplus;"):
            assert cppyy.gbl.std.optional
            assert cppyy.gbl.std.nullopt

            cppyy.cppdef("""
                enum Enum { A = -1 };
                bool callopt(std::optional<Enum>) { return true; }
            """)

            a = cppyy.gbl.std.optional[cppyy.gbl.Enum]()
            assert cppyy.gbl.callopt(a)

            c = cppyy.gbl.nullopt
            assert cppyy.gbl.callopt(c)
