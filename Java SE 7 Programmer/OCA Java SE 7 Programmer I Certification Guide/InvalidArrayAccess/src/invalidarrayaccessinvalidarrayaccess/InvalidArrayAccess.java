package invalidarrayaccessinvalidarrayaccess;

public class InvalidArrayAccess {
	public static void main(String args[]) {
	String[] students = {"Shreya", "Joseph"};
	
	try {
		System.out.println(students[5]);
	}
	catch (ArrayIndexOutOfBoundsException e){
		System.out.println("Exception");
	}
		System.out.println("All seems to be well");
	}
}
