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

assert fake_hash_map["HELLO WORLD"] == "Hello";

var numbers = [];

for (var i = 0; i < 100; i = i + 1) {
  push(numbers, i);
}

while len(numbers) {

  when numbers[len(numbers) - 1] {
    -1..100 -> {
      print "It's a number between 0 and 99";
    }


    "a string??" | "another string" | -100 -> {
      print "It's a string of value `a string??` or a string of value 'another string' or a number of value -100";
    }

    nothing -> print "Not a number between 0 and 99 and not `a string??` and not 'another string' and not a number of value -100";
  }

  assert pop(numbers) == len(numbers);
}
```
