package multiplereturn2;

class MultipleReturn2 {
	StringBuilder getStringBuilder() {
		StringBuilder returnVal = new StringBuilder("10");
		
		try {
			String[] students = {"Harry", "Paul"};
			System.out.println(students[5]);
		}
		catch (Exception e) {
			System.out.println("About to return :" + returnVal);
			return returnVal;
		}
		finally {
			returnVal.append("10");
			System.out.println("Return value is now :" + returnVal);
		}
		return returnVal;
	}
	
	public static void main(String args[]) {
		MultipleReturn2 var = new MultipleReturn2();
		System.out.println("In Main:" + var.getStringBuilder());
	}
}
