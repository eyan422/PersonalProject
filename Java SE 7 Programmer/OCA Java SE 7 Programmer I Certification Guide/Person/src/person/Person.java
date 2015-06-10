package person;

class Person {
	private String name;
	
	Person(String newName) {
		name = newName;
	}
	
	public String getName() {
		return name;
	}
	
	public void setName(String val) {
		name = val;
	}
	
	public static void swap(Person p1, Person p2) {
		Person temp = p1;
		p1 = p2;
		p2 = temp;
	}
	
	public static void main(String args[]) {
		Person person1 = new Person("John");
		Person person2 = new Person("Paul");
		
		System.out.println(person1.getName()
		+ ":" + person2.getName());
		swap(person1, person2);
		
		System.out.println(person1.getName()
		+ ":" + person2.getName());
	}
}
