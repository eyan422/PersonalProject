package superpointer;

class Employee {
	String name;
}

class Programmer extends Employee {
	String name;
	
	void setNames() {
		this.name = "Programmer";
		super.name = "Employee";
	}

	void printNames() {
		System.out.println(super.name);
		System.out.println(this.name);
	}

}

class Super {
	public static void main(String[] args) {
		Programmer programmer = new Programmer();
		programmer.setNames();
		programmer.printNames();
	}
}