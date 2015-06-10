package employees;

class Employee {
	String name;
	int age;
	
	Employee() {
		//Recursive constructor invocation Employee(String, int)
		this("Shreya", 10);
	}
	
	Employee (String newName, int newAge) {
		//this();
		name = newName;
		age = newAge;
	}
	
	void print(){
		print(age);
	}
	
	void print(int age) {
		print();
	}
}
