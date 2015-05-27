package constructerini;

//public class ConstructerIni {
class Employee {
	String name;
	int age;
	
	Employee() 
	{
		//this ();
		this("Frank", 12);
	}
	
	Employee (String newName, int newAge) 
	{
		name = newName;
		age = newAge;
	}
}

class ConstructerIni
{
	
	public static void main(String args[])
	{
		Employee e = new Employee();
		System.out.println(e.name);
		System.out.println(e.age);
	}
}