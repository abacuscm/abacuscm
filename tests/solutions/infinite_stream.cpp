#include <iostream>

using namespace std;

int main()
{
	cout.exceptions(ios::failbit);
	while (1)
		cout << "foobarbazblah\n";
	return 0;
}

