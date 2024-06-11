#include <bits/stdc++.h>
using namespace std;

int main(){
    auto sum = [&](int a,int b) -> auto {return a + b;};

    cout << sum(1,2) << endl;

}
