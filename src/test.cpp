#include <iostream>

int main(void)
{

    std::cout << static_cast<int>('a') << std::endl;
    std::cout << static_cast<int>('z') << std::endl;
    std::cout << static_cast<int>('A') << std::endl;
    std::cout << static_cast<int>('Z') << std::endl;
    
    return 0;
}