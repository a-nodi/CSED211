#include<stdio.h>
#include<time.h>

int main()
{
	int i = 0;
	clock_t begin = clock();
	for (; i<1000000000; i++)
		if (i % 100000000 == 0)
			printf("loading\n");
	clock_t end = clock();
	double spent_time = (double)(end - begin);
	printf("spent time: %f\n", spent_time);
	return 0;
}


