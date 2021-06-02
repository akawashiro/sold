class X {
public:
    double f() { return i_static + d_static; }

private:
    static int i_static;
    static double d_static;
};

double func();
