from time import time

def fibonacci(n):
    fib = 0
    if type(n) != int:
        print("Wrong input type!")
        return 0
    if n < 2:
        fib = n
    else:
        fib = fibonacci(n - 1) + fibonacci(n - 2)
    return fib

start = time()
print(fibonacci(35))
end = time()
print(end - start)
