package twistIntale4;

class Employee {
	//INSERT CODE HERE// 
	String run() {
	System.out.println("Emp-run");
		return null;
	}
}
	
class Programmer extends Employee{
	String run() {
		System.out.println("Programmer-run");
		return null;
	}
}

class TwistInTale4 {
	public static void main(String args[]) {
		new Programmer().run();
	}
}