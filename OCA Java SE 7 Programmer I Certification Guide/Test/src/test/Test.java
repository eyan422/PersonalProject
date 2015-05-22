package test;

class Person {
	Person(String value) {}
	Person() {}
}

class Employee extends Person {}

class Test {
	public static void main(String args[]) {
		Employee e = new Employee();
	}
}
