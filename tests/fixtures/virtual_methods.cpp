// Classes with virtual methods for testing
class Base {
public:
    virtual void process() {
        // Base implementation
    }

    virtual int compute(int x) {
        return x * 2;
    }

    virtual ~Base() {}
};

class Derived : public Base {
public:
    void process() override {
        // Derived implementation
    }

    int compute(int x) override {
        return x * 3;
    }
};
