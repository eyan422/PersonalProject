package multiplereturn1;

class MultipleReturn1 {
	int getInt() {
		int returnVal = 10; 
		
		try {
			String[] students = {"Harry", "Paul"};
			System.out.println(students[5]);
		}
		catch (Exception e) {
			System.out.println("About to return :" + returnVal);
			return returnVal;
		}
		finally {
			returnVal += 10; 
			System.out.println("Return value is now :" + returnVal);
		}
		return returnVal;
	}
	
	public static void main(String args[]) {
		MultipleReturn1 var = new MultipleReturn1();
		System.out.println("In Main:" + var.getInt());
	}
}
