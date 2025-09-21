// FPrueba del algoritmo floyd

#include <stdio.h>
//Tesssst

// numero de nodos
#define nV 5

#define INF 999

void printMatrix(int matrix[][nV]);


void floyd(int graph[][nV]) {
  int matrix[nV][nV], i, j, k;

  for (i = 0; i < nV; i++)
    for (j = 0; j < nV; j++)
      matrix[i][j] = graph[i][j];


  for (k = 0; k < nV; k++) {
    for (i = 0; i < nV; i++) {
      for (j = 0; j < nV; j++) {
        if (matrix[i][k] + matrix[k][j] < matrix[i][j])
          matrix[i][j] = matrix[i][k] + matrix[k][j];
      }
    }
    printMatrix(matrix);
    printf("\n");
  }
  printMatrix(matrix);
}

void printMatrix(int matrix[][nV]) {
  for (int i = 0; i < nV; i++) {
    for (int j = 0; j < nV; j++) {
      if (matrix[i][j] == INF)
        printf("%4s", "INF");
      else
        printf("%4d", matrix[i][j]);
    }
    printf("\n");
  }
}

int main() {
  int graph[nV][nV] = {{0, 6, INF, 4, 7},
             {9, 0, 7, INF, INF},
             {INF, 5, 0, INF, 14},
             {8, 1, INF, 0, 15},
             {2, INF, 2, 19, 0}
            };
  floyd(graph);
}