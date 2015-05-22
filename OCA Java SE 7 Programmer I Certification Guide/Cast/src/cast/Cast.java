package cast;

public class Cast {
	public static void main(String args[])
	{
		Interviewer interviewer = new HRExecutive();
		//interviewer.specialization = new String[] {"Staffing"};
		((HRExecutive)interviewer).specialization = new String[] {"Staffing"};
	}
}

class Employee {}

interface Interviewer {
	public void conductInterview();
}

class HRExecutive extends Employee implements Interviewer {
	String[] specialization;
		public void conductInterview() {
			System.out.println("HRExecutive - conducting interview");
	}
}

class Manager implements Interviewer{
	int teamSize;
	public void conductInterview() {
		System.out.println("Manager - conducting interview");
	}
}
