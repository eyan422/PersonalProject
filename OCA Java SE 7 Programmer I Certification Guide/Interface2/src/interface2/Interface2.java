package interface2;

interface BaseInterface1 
{
	String getName();
}

interface BaseInterface2 
{
	String getName();
}

interface MyInterface extends BaseInterface1, BaseInterface2 
{}

class Employee implements MyInterface 
{
	String name;
	
	Employee(String name)
	{
		this.name = name;
	}
	
	public String getName() {
		return name;
	}
}

public class Interface2
{
	public static void main(String args[])
	{
		Employee e = new Employee("Frank");
		System.out.println(e.getName());
	}
}