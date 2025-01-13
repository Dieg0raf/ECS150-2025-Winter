#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    std::string str1 = "hello";

    // Reference - must be initialized when declared
    std::string& ref = str1; // Reference to str1

    // Pointer - can be initialized later
    std::string* ptr = &str1; // Pointer to str1

    std::cout << "Original values:\n";
    std::cout << "str1: " << str1 << "\n";
    std::cout << "ref:  " << ref << "\n";
    std::cout << "ptr:  " << *ptr << "\n\n";

    // Changing through reference
    ref = "world"; // Direct assignment
    std::cout << "After ref change:\n";
    std::cout << "str1: " << str1 << "\n";
    std::cout << "*ptr: " << *ptr << "\n\n";

    // Changing through pointer
    *ptr = "pointer"; // Need dereferencing
    std::cout << "After ptr change:\n";
    std::cout << "str1: " << str1 << "\n";

    return 0;
}