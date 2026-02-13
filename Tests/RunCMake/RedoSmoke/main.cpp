#include <iostream>

#include "generated.h"
#include "lib.h"

int main()
{
  std::cout << greet() << " (" << GENERATED_MESSAGE << ")\n";
  return 0;
}

