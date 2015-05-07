package inheritance1;

public class Inheritance1 {
	public static void main(String args[])
	{
		Programmer p = new Programmer();
		Manager m = new Manager();
		
		p.accessBaseClassMembers();
		m.accessBaseClassMembers();
		
		p.sendInvitation(p);
		m.sendInvitation(m);
	}
}

class Employee {
	String name;
	String address;
	String phoneNumber;
	float experience;
	
	void sendInvitation(Employee emp) 
	{
		System.out.println("Send invitation to " + emp.name);
	}
}

class Manager extends Employee {
	int teamSize;
	
	void accessBaseClassMembers() 
	{
		name = "Manager";
	}
	
	void reportProjectStatus() 
	{
		
	}
}

class Programmer extends Employee {
	String[] programmingLanguages;
	
	void writeCode() {}
	
	void accessBaseClassMembers() 
	{
		name = "Programmer";
	}
}
