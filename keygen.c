#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char *argv[])
{
	if(argc <= 1)
	{
		printf("Enter the size, silly.\n");
		return 1; // user didnt enter the size of the key
	}

	srand(time(NULL));
	int keylength = atoi(argv[1]);

	int i;

	for(i = 0; i < keylength; i++)
	//creating a random key and entering it into the file
	{
		int letter = rand()%26 + 65;
		printf("%c", (char)letter);

	}

	printf("\n");

	return 0;
}
