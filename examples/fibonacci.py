from time import clock

def fibonacci(n):
    fib = 0
    if n < 2:
        fib = n
    else:
        fib = fibonacci(n - 1) + fibonacci(n - 2)
    return fib

start = clock()
print(fibonacci(35))
end = clock()
print(end - start)
