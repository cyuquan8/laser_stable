y = 3

class c1:
    def __init__(self):
        self.x = y
    def f1(self):
        self.x = self.x + 1


def f2():

    foo = c1()
    foo.f1()

    global y
    
    y += 1

def f3():
    
    foo = c1()
    foo.f1()

    global y

    y += 1
    y += 1

f2()
f3()
print(y)