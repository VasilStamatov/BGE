#include <util/Timer.h>

#include <iostream>

// The size of the square matrix. Actual allocated elements number is
// matrixSize^2. If the size of the allocated elements cause a crash from stack
// overflow, reduce the size here.
constexpr int matrixSize = 500;

void ProcessByRow()
{
  bool boolMatrix[matrixSize][matrixSize];

  bge::Timer timer;
  for (size_t row = 0; row < matrixSize; ++row)
  {
    for (size_t col = 0; col < matrixSize; ++col)
    {
      boolMatrix[row][col] = true;
    }
  };
  std::cout << "Time it took to process by row: " << timer.GetElapsedMilli()
            << " millis" << std::endl;
}

void ProcessByColumn()
{
  bool boolMatrix[matrixSize][matrixSize];

  bge::Timer timer;
  for (size_t row = 0; row < matrixSize; ++row)
  {
    for (size_t col = 0; col < matrixSize; ++col)
    {
      boolMatrix[col][row] = true;
    }
  }
  std::cout << "Time it took to process by column: " << timer.GetElapsedMilli()
            << " millis" << std::endl;
}

void ProcessPointerArray()
{
  bool* pointers[matrixSize][matrixSize];
  // populate the array first
  for (size_t row = 0; row < matrixSize; ++row)
  {
    for (size_t col = 0; col < matrixSize; ++col)
    {
      pointers[row][col] = new bool(true);
    }
  }

  bge::Timer timer;
  for (size_t row = 0; row < matrixSize; ++row)
  {
    for (size_t col = 0; col < matrixSize; ++col)
    {
      *pointers[row][col] = false;
    }
  }
  std::cout << "Time it took to process pointer array: "
            << timer.GetElapsedMilli() << " millis" << std::endl;
}

int main()
{
  // Benchmarks in release build

  // Time it took to process by column: 0.008247 millis
  // Time it took to process by column: 0.030067 millis
  // Time it took to process by column: 0.031197 millis
  // Time it took to process by column: 0.028698 millis
  // Time it took to process by column: 0.020808 millis
  // Average: 0.0238 ms
  ProcessByColumn();

  // Time it took to process by row: 0.00012 millis
  // Time it took to process by row: 0.000666 millis
  // Time it took to process by row: 0.000517 millis
  // Time it took to process by row: 0.001023 millis
  // Time it took to process by row: 0.000283 millis
  // Average: 0.0005218 ms
  ProcessByRow();

  // Time it took to process pointer array: 0.753056 millis
  // Time it took to process pointer array: 0.772343 millis
  // Time it took to process pointer array: 1.30804 millis
  // Time it took to process pointer array: 0.77322 millis
  // Time it took to process pointer array: 0.768705 millis
  // Average: 0.8750728 ms
  ProcessPointerArray();
}