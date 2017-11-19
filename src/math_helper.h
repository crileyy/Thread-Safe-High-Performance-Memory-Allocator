#ifndef MATH_HELPER_H
#define MATH_HELPER_H

int ciel(double y)
{
    int remainder = (int) (y * 10);
    if (remainder > 0)
        return (int) y + 1;
    else
        return (int)y;

}

int power(double base, double power)
{
    int result = 1;
    int i;
    for (i = 0; i < power; i++)
    {
        result *= (int)base;
    }
    return result;
}

long ilog2(long value)
{
    // given from Nat on Piazza
    long result = 0;
    long cur_val = value;
    while (cur_val > 1)
    {
        ++result;
        cur_val >>= 1;
    }
    if (power(2, result) < value)
        return result + 1;
    return result;
}

#endif
