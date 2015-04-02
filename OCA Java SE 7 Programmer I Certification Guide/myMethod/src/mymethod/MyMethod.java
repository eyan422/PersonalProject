package mymethod;

class Exam {
	String name;
	public void setName(String newName) {
		name = newName;
	}
}

public class MyMethod {
	public void myMethod() {
		int result = 88;
		
		if (result > 78) 
		{
			Exam myExam1 = new Exam();
			myExam1.setName("Android");
			System.out.println(myExam1.name);
		}
		else 
		{
			Exam myExam2 = new Exam();
			myExam2.setName("MySQL");
			System.out.println(myExam2.name);
		}
		
	}
	
	public static void main(String args[]){
		MyMethod m = new MyMethod();
		m.myMethod();
	}
}
