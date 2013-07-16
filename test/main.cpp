/*
 * main.cpp - Unit tests to excersise IOC container
 *
 * Copyright (c) 2012 Nicholas A. Smith (nickrmc83@gmail.com)
 * Distributed under the Boost software license 1.0, 
 * see boost.org for a copy.
 */

#include <ioc_container/ioc.h>
#include <iostream>
#include <memory>
#include <vector>
#include <stdint.h>
#include <memory>
#include <cstring>

// Possible status of tests
enum TestStatus
{
    TS_Success = 0,
    TS_Unknown,
    TS_Registration_Error,
    TS_Unknown_Registration,
    TS_Resolution_Error
};

static inline bool TestSucceeded( TestStatus Status )
{
    return Status == TS_Success;
}

static inline bool TestFailed( TestStatus Status )
{
    return !TestSucceeded( Status );
}

// Test function signature
typedef TestStatus (*TestFuncSignature)();
// Test function adapter.
class TestFunctionObject
{
    private:
        std::string Name;
        TestFuncSignature Func;
    public:
        TestFunctionObject( const std::string &TestName, 
                TestFuncSignature FuncIn ) :
            Name( TestName ), Func( FuncIn )
    {
    }

        const std::string &GetName() const
        {
            return Name;
        }

        TestStatus Execute() const
        {
            TestStatus Result = TS_Unknown;
            if( Func )
            {
                Result = Func();
            }

            return Result;
        }
};

// Helper exception printer
static void PrintException( const char *Function, const std::exception &e )
{
    std::cout << "Exception in " 
        << Function << ", " 
        << e.what() << std::endl;
}

static void PrintTestStart( const TestFunctionObject &Obj )
{
    std::cout << "Beginning " << Obj.GetName() << std::endl;
}

static void PrintTestSuccess( const TestFunctionObject &Obj )
{
    std::cout << Obj.GetName() << " success" << std::endl;
}

static void PrintTestFailure( const TestFunctionObject &Obj )
{
    std::cout << Obj.GetName() << " failure" << std::endl;
}


// Counters to measure the number of
// constructed and destructed types.
static size_t ConstructedCount;
static size_t DestructedCount;

static void ResetCounters()
{
    ConstructedCount = 0;
    DestructedCount = 0;
}

// Generic Interface for use in testing
struct InterfaceType
{
    virtual ~InterfaceType()
    {
    }

    virtual bool Success() const
    {
        return false;
    }
};

// Generic concretion for use in testing
struct Concretion : public InterfaceType
{
    Concretion() : InterfaceType()
    {
        ConstructedCount++;
    }

    ~Concretion()
    {
        DestructedCount++;
    }

    bool Success() const
    {
        return true;
    }
};

struct ComplexConcretion : public Concretion
{
    Concretion *InnerInstance;

    ComplexConcretion( Concretion *Instance )
        : InnerInstance( Instance )
    {
    }
};

// Concretion that throws in its constructor
// to help test if objects generated by IOC
// are cleaned-up during a failed resolution.
struct ThrowingConcretion : public InterfaceType
{
    ThrowingConcretion()
        : InterfaceType()
    {
        std:: cout << "Throwing constuctor" << std::endl;
        throw std::bad_exception();
    }
};

struct CompositeType
{
    InterfaceType *Interface;
    Concretion *Concrete;

    CompositeType( InterfaceType *InterfaceIn, Concretion *ConcreteIn )
        : Interface( InterfaceIn ), Concrete( ConcreteIn )
    {
    }
};

// The unit tests

// Test we can create and IOC::Container
static TestStatus TestConstructor()
{
    TestStatus Result = TS_Unknown;
    ioc::container *Container = NULL;
    try
    {
        Container = new ioc::container();
        Result = TS_Success;

        // Delete the container
        delete Container;
        Container = NULL;
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

// Test we can destroy and IOC::Container
static TestStatus TestDestructor()
{
    TestStatus Result = TS_Unknown;
    ioc::container *Container = NULL;
    try
    {
        Container = new ioc::container();
        delete Container;
        Container = NULL;
        Result = TS_Success;
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

// Test if we can just Register a type without an
// exception
static TestStatus TestRegister()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;

    try
    {
        Container.register_type<InterfaceType, Concretion>();
        Result = TS_Success;
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    } 

    return Result;
}

// Test the TypeIsRegistered function.
static TestStatus TestTypeIsRegistered()
{
    TestStatus Result = TS_Unknown_Registration;
    ioc::container Container;
    try
    {
        Container.register_type<InterfaceType, Concretion>();
        if( Container.type_is_registered<InterfaceType>() )
        {
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
    }

    return Result;
}

// Attempt to register a simple class type which
// has no constructor arguments. Successful
// registration requires successful resolution
// for testing.
static TestStatus TestRegisterResolve()
{   
    ioc::container Container;
    TestStatus Result = TS_Registration_Error;
    try
    {
        // Register
        std::cout << "Registering Concretion as Interface" << std::endl;
        Container.register_type<InterfaceType, Concretion>();
        Result = TS_Resolution_Error;
        // Resolve
        std::cout << "Resolving Interface" << std::endl;
        std::shared_ptr<InterfaceType> Value = Container.resolve<InterfaceType>();
        if( Value.get() && Value->Success() )
        {
            std::cout << "Successfully resolved Interface" << std::endl;
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;
}

// Test if we can Register and Resolve a complex type.
// A complex type is one which requires constructor
// injection
static TestStatus TestRegisterResolveComplexType()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;
    try
    {
        ResetCounters();

        // First register a simple type
        Container.register_type<Concretion, Concretion>();
        // Second register a type which requires an instance
        // of our simple type. This forces the Resolver
        // to find a simple type before it attempts to
        // construct our complex type.
        Container.register_type<ComplexConcretion, 
            ComplexConcretion, 
            Concretion *>();
        Result = TS_Resolution_Error;

        // Attempt to resolve the complex type
        std::shared_ptr<ComplexConcretion> Inst = Container.resolve<ComplexConcretion>();

        if( Inst.get() )
        {
            Result = TS_Success;
            std::cout << "Successfully resolved complex type" << std::endl;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;
}

// Try and Register a type with a name
static TestStatus TestRegisterWithName()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;
    try
    {
        Container.register_type_with_name<InterfaceType, Concretion>( "ThisName" );
        if( Container.type_is_registered<InterfaceType>( "ThisName" ) )
        {
            Result = TS_Success;
        }        
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;
}

// Try and the same type more than once. We expect
// to catch a registration exception.
static TestStatus TestRegisterTypeMoreThanOnce()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;
    try
    {
        Container.register_type<InterfaceType, Concretion>();
        try
        {
            Container.register_type<InterfaceType, Concretion>();
            std::cout << "Why?" << std::endl;
        }
        catch( const ioc::registration_exception &e )
        {
            // We expect to catch an exception here
            PrintException( __func__, e );
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

// Test if we can register two identifical types with the
// same name. We expect to catch a registration exception.
static TestStatus TestRegisterTypeWithNameMoreThanOnce()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;
    try
    {
        Container.register_type_with_name<InterfaceType, Concretion>( "ThisName" );
        try
        {
            Container.register_type_with_name<InterfaceType, Concretion>( "ThisName" );
        }
        catch( const ioc::registration_exception &e )
        {
            // We expect to catch an exception here
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;
}

// Test if we can register two different types with the same
// name.
static TestStatus TestRegisterMoreThanOneTypeWithTheSameName()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;

    try
    {
        Container.register_type_with_name<InterfaceType, Concretion>( "ThisName" );
        Container.register_type_with_name<Concretion, Concretion>( "ThisName" );
        Result = TS_Success;
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;
}

// Test if types which are automatically resolved, during resolution of
// a complex, are de-aalocated if an exception is thrown during the
// constructor of a complex type.
static TestStatus TestResolveComplexTypeClearsUpConstructedTypesOnError()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container Container;
    try
    {
        Container.register_type<Concretion, Concretion>();
        Container.register_type<InterfaceType, ThrowingConcretion>();
        Container.register_type<CompositeType, CompositeType, InterfaceType, Concretion>();
        // We expect to catch an error but the constructor variables for
        // Throwing concretion to have been deleted.
        try
        {
            std::shared_ptr<CompositeType> r = Container.resolve<CompositeType>();
        }
        catch(const std::exception &e)
        {
            PrintException( __func__, e );
        }
        // We expect a single concretion
        if( ( ConstructedCount == 1 ) && ( DestructedCount == 1 ) )
        {
            std::cout << "Constructed " << ConstructedCount << 
                ", Destructed " << DestructedCount << std::endl;
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

static TestStatus TestResolveInterfaceByName()
{
    TestStatus Result = TS_Resolution_Error;
    const std::string registration_name = "TestName";
    ioc::container container;
    try
    {
        container.register_type_with_name<Concretion, Concretion>( registration_name );
        std::shared_ptr<Concretion> r = container.resolve_by_name<Concretion>( registration_name );
        if( r.get() != NULL )
        {
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

static TestStatus TestRemoveRegistration()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container container;
    try
    {
        container.register_type<Concretion, Concretion>();
        if( container.remove_registration<Concretion>() )
        {
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;
}

static TestStatus TestRemoveRegistrationByName()
{
    TestStatus Result = TS_Registration_Error;
    const std::string registration_name = "TestName";
    ioc::container container;
    try
    {
        container.register_type_with_name <Concretion, Concretion>( registration_name );
        if( container.remove_registration_by_name<Concretion>( registration_name ) )
        {
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }
    return Result;

}

// Test delegate for generating a concretion
static Concretion *CreateConcretion()
{
    return new Concretion();
} 

static TestStatus TestRegisterDelegate()
{
    TestStatus Result = TS_Registration_Error;
    ioc::container container;
    try
    {
        container.register_delegate<Concretion>( CreateConcretion );
        if( container.type_is_registered<Concretion>() )
        {
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

static TestStatus TestRegisterDelegateWithName()
{
    TestStatus Result = TS_Registration_Error;
    const std::string registration_name = "TestName"; 
    ioc::container container;
    try
    {
        container.register_delegate_with_name<Concretion>( registration_name, CreateConcretion );
        if( container.type_is_registered<Concretion>( registration_name ) )
        {
            Result = TS_Success;
        }
    }
    catch( const std::exception &e )
    {
        PrintException( __func__, e );
    }

    return Result;
}

// Helper macro for registering tests with a name.
#define REGISTER_TEST( v, x ) ( v.push_back( TestFunctionObject( #x, &x ) ) ) 
// Register all test functions within this function
// call.
static std::vector<TestFunctionObject> GetRegisteredTests()
{
    std::vector<TestFunctionObject> Result;
    REGISTER_TEST( Result, TestConstructor );
    REGISTER_TEST( Result, TestDestructor );
    REGISTER_TEST( Result, TestRegister );
    REGISTER_TEST( Result, TestTypeIsRegistered );
    REGISTER_TEST( Result, TestRegisterResolve );
    REGISTER_TEST( Result, TestRegisterResolveComplexType );
    REGISTER_TEST( Result, TestRegisterWithName );
    REGISTER_TEST( Result, TestRegisterTypeMoreThanOnce );
    REGISTER_TEST( Result, TestRegisterTypeWithNameMoreThanOnce );
    REGISTER_TEST( Result, TestRegisterMoreThanOneTypeWithTheSameName );
    REGISTER_TEST( Result, TestResolveComplexTypeClearsUpConstructedTypesOnError );
    REGISTER_TEST( Result, TestResolveInterfaceByName );
    REGISTER_TEST( Result, TestRemoveRegistration );
    REGISTER_TEST( Result, TestRemoveRegistrationByName );
    REGISTER_TEST( Result, TestRegisterDelegate );
    REGISTER_TEST( Result, TestRegisterDelegateWithName );
    return Result;
}
#undef REGISTER_TEST

// Execute given test
static int ExecuteTests( const std::vector<TestFunctionObject> &Tests )
{
    // Global status counters    
    size_t SuccessCount = 0;
    size_t FailureCount = 0;

    for( std::vector<TestFunctionObject>::const_iterator i = Tests.begin();
            i != Tests.end(); ++i )
    {
        // Print test separator pattern
        std::cout << "???????????????????????????????????????????" << std::endl;
        PrintTestStart( *i );
        TestStatus Result = TS_Unknown; 

        // Reinit global variables for each test
        ResetCounters();
        try
        {
            // Execute test function
            Result = (*i).Execute();
        }
        catch( const std::exception &e )
        {
            PrintException( __func__, e );
        }

        // Check for success
        if( TestSucceeded( Result ) )
        {
            SuccessCount++;
            PrintTestSuccess( *i );
        }
        else
        {
            FailureCount++;
            PrintTestFailure( *i );
        }

        // newline for readability
        std::cout << std::endl;
    }

    // Print final results to the screen
    std::cout << "*******************************************" << std::endl;
    std::cout << "Final test run results: Success " << 
        SuccessCount << ", Failure " << FailureCount << std::endl;

    // A single failure constitutes an overall failure
    return FailureCount;
}

// Execute methods
int main( int argc, char **argv )
{
    // Print commandline variables to std::out
    std::cout << "This application was executed with the following arguments" << std::endl;
    for( int i = 0; i < argc; i++ )
    {
        std::cout << (i+1) << ") " << argv[i] << std::endl;
    }

    std::cout << std::endl;

    // Register functions for test
    std::cout << "Obtaining registered tests" << std::endl << std::endl;
    std::vector<TestFunctionObject> TestFunctions = GetRegisteredTests();

    // Execute tests
    std::cout << "Executing registered tests" << std::endl << std::endl;;
    int Result = ExecuteTests( TestFunctions );	

    // Success is no errors
    return Result;
}	
