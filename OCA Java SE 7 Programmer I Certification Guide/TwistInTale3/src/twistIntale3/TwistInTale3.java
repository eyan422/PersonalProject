package twistIntale3;

class Employee 
{
	String name = "Emp";
	String address = "EmpAddress";
}

class Programmer extends Employee
{
	String name = "Prog";
	
	void printValues() 
	{
		System.out.print(this.name + ":");
		System.out.print(this.address + ":");
		System.out.print(super.name + ":");
		System.out.print(super.address);
	}
}

class TwistInTale3 
{
	public static void main(String args[]) 
	{
		new Programmer().printValues();
	}
}