package myclass;

class MyClass {
	
	MyClass(){
		System.out.println("Constructor");
	}
	
	double calcAverage(double marks1, int marks2) {
		return (marks1 + marks2)/2.0;
	}
	
	double calcAverage(int marks1, double marks2) {
		return (marks1 + marks2)/2.0;
	}
	
	public static void main(String args[]) {
		MyClass myClass = new MyClass();
		/*Error
		 * myClass.calcAverage(2, 3)
		 * */
		System.out.println(myClass.calcAverage(2, 3.0));
		System.out.println(myClass.calcAverage(2.0, 3));
	}
}
