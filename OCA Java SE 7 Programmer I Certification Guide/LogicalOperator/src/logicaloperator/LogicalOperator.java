package logicaloperator;

public class LogicalOperator {
	
	public static void main(String arg[]){
		int a = 10;
		int b = 20;
		int marks = 8;
		int total = 10;
		
		System.out.println(a > 20 && b > 10);
		System.out.println(a > 20 || b > 10);
		System.out.println(! (b > 10));
		System.out.println(! (a > 20));
		
		System.out.println(total < marks && ++marks > 5);
		System.out.println(marks);
		System.out.println(total == 10 || ++marks > 10);
		System.out.println(marks);
		
		String name = "hello";
		if (name != null && name.length() > 0)
		System.out.println(name.toUpperCase());
	}
}
