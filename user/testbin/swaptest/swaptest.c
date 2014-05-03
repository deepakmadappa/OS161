#include<stdio.h>

int
main()
{
	int arr[231000],sum=0;
	for(int i=0; i < 231000; i++)
		arr[i]=i+10;
	for(int i=0; i < 231000; i++)
		sum += arr[i];
	printf("sum=%d", sum);
return 0;		
}
