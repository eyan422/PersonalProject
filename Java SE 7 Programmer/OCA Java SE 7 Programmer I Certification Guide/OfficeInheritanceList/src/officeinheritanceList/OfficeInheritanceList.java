package officeinheritanceList;

class Employee {
	String name;
	String address;
	String phoneNumber;
	float experience;
}

class HRExecutive extends Employee implements Interviewer {
	String[] specialization;
	
	public void conductInterview() {
		System.out.println("HRExecutive - conducting interview");
	}
}

class Manager extends Employee implements Interviewer {
	String[] rank;
	
	public void conductInterview() {
		System.out.println("Manager - conducting interview");
	}
}

interface Interviewer {
	public void conductInterview();
}

class OfficeInheritanceList {
	public static void main(String args[]) {
		
		Interviewer[] interviewers = new Interviewer[2];
		interviewers[0] = new Manager();
		interviewers[1] = new HRExecutive();
		
		for (Interviewer interviewer : interviewers) {
			interviewer.conductInterview();
		}
	}
}
