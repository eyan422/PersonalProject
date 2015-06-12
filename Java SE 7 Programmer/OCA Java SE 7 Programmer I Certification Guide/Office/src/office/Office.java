package office;

class Employee {
	private String name;
	int age;
	
	/*
	Employee() {
		age = 22;
	}
	*/
	
	public void setName(String val) {
		name = val;
	}
	
	public void printEmp() {
		System.out.println("name = " + name + " age = " + age);
	}
}

class Office {
	public static void main(String args[]) {
		Employee e1 = new Employee();
		Employee e2 = new Employee();
		
		//e1.name = "Selvan";
		e2.setName("Harry");
		e1.printEmp();
		e2.printEmp();
	}
}