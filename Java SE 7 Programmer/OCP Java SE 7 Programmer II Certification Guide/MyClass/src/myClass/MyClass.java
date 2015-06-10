package myClass;

class MyClass {
	static double calcAverage(double marks1, int marks2) {
		return(marks1 + marks2)/2;
	}
	
	static double calcAverage(int marks1, double marks2) {
		return(marks1 + marks2)/2;
	}
	
	static double calcAverage(int marks1, int marks2)	{
		return(marks1 + marks2)/2;
	}
	
	public static void main(String[] args) {
		calcAverage(2, 3);
	}
}
