#include "ringbuffer.h"
#include <iostream>

using namespace std;

int main()
{
    char *szString = "this is a test string";
    RingBuffer ring(10);
    ring.AddData(szString);
    int iNum = 0;
    iNum = ring.GetNum();
    cout << "ring num is:" << iNum << endl;
    char *szResult = ring.PopFront();
    cout << szResult << endl;
    char *szS1 = "1";
    char *szS2 = "2";
    char *szS3 = "3";
    char *szS4 = "4";
    char *szS5 = "5";
    char *szS6 = "6";
    char *szS7 = "7";
    char *szS8 = "8";
    char *szS9 = "9";
    char *szS10 = "10";
    char *szS11 = "11";
    ring.AddData(szS1);
    ring.AddData(szS2);
    ring.AddData(szS3);
    ring.AddData(szS4);
    ring.AddData(szS5);
    ring.AddData(szS6);
    ring.AddData(szS7);
    ring.AddData(szS8);
    ring.AddData(szS9);
    int iAddResult = ring.AddData(szS10);
    cout << "add s10 result:" << iAddResult << endl;
    iAddResult = ring.AddData(szS11);
    cout << "add s11 result:" << iAddResult << endl;
    szResult = ring.PopFront();
    cout << "popfront should get 1, the result is:" << szResult << endl;
    return 0;
}
