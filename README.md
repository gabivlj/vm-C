## QW lang, a toy compiler and VM based on clox

Created based on the magnificent piece of work that is Crafting an Interpreter,
extended to have even more features like hashmaps, arrays, switch statements, and
overall better performance like using jump tables or one allocation for strings!

(I may add even more features in the future)

For example, this is a piece of code that you can write in QW

```kt

class EmptyClass {}

var fake_hash_map = EmptyClass();

fake_hash_map["HELLO WORLD"] = "Hello";

print fake_hash_map["HELLO WORLD"];

var numbers = [];

for (var i = 0; i < 100; i = i + 1) {
  push(numbers, i);
}

while len(numbers) {

  when numbers[len(numbers) - 1] {
    -1..100 -> {
      print "It's a number between 0 and 99";
    }

    "a string??" -> {
      print "It's a string of value `a string??`";
    }

    nothing -> print "Not a number between 0 and 99 and not `a string??`";
  }

  assert pop(numbers) == len(numbers);
}
```
