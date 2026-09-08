```C
 if unlikely ((lf | cr) & 0xe000000000000000ull)
                if (('\xd' is b[-3]) && ('\xa' is b[-2]) && ('\xd' is b[-1]) && ('\xa' is b[0]))
                    return j * N + 4;

//  cr  lf  cr  lf 
//  1   1   1   1
// test last 4 bytes
switch ((lf | cr) & 0b111)
{
	likely case: 0;
	case 0b1:
        read = 2
    case 0b11
        read = 1
}
```