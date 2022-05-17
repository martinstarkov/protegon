template < class Tr, class... Args >
class Button
{
    typedef Tr(*eventFunction)(Args...);

    Button( eventFunction funcPtr ) : m_funcPtr( funcPtr ){}

    ~Button(){ m_funcPtr = NULL; }

    void operator() ( Args&... args ) const {
        (m_funcPtr)(args...);
    }

    private:
        eventFunction m_funcPtr;
};

void myFunction1( int, double ) { /* do something */ }
void myFunction2() { /* do something */ }
/*
Button<void, int, double> myButton1( &myFunction1 );
myButton1( 10, 20.5 );  // invoke the function registered with two arguments

Button<void> myButton2( &myFunction2 );
myButton1(); // invoke the function registered without argument
*/
template < class C, class Tr, class... Args >
class ButtonB
{
    typedef Tr(C::*eventMethod)(Args...);
    ~Button(){}

    Button( C& target, eventMethod method ) : m_target(target), m_method(method) {}

    bool operator () ( Args&... args ) const {
        (m_target.*(m_method))(args...);
    }

    private:
        eventMethod m_method;
        C& m_target;
};

class MyClass
{
    void method1(){};
    void method2( int, double ){};
};
/*
MyClass myC1;

ButtonB<MyClass, void> myButton1( myC1, &MyClass::method1 );
myButton1(); // invoke the registered method using the registered class instance.

ButtonB<MyClass, void, int, double> myButton2( myC1, &MyClass::method2 );
myButton2( 15, 170.5 ); // invoke the registered method using the registered class instance.
*/