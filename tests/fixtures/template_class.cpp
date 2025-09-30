// Template class for testing
template<typename T>
class Container {
private:
    T value;

public:
    Container(T val) : value(val) {}

    T get() const {
        return value;
    }

    void set(T val) {
        value = val;
    }
};
