package inheritance2;

public class Inheriance2 {
	public static void main(String args[]) 
	{
		//new Programmer ("Harry");
		String name = new Programmer ("Harry").getName();
		System.out.println("Name: "+name);
	}
}

class Employee 
{
	String name;
	String address;
	protected String phoneNumber;
	public float experience;
}

class Programmer extends Employee 
{
	Programmer (String val) 
	{
		name = val;
	}
	
	String getName() 
	{
		return name;
	}
}