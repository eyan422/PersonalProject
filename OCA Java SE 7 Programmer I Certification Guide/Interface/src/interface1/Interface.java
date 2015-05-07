package interface1;

/*
interface Trainable {
	public void attendTraining();
}
*/

interface Trainable {
	public void attendTraining(String[] trainingSchedule);
}

interface Interviewer {
	public void conductInterview();
}

class Employee {
	String name;
	String address;
	String phoneNumber;
	float experience;
}

class Manager extends Employee implements Interviewer, Trainable
{
	int teamSize;
	void reportProjectStatus() {}
	
	public void conductInterview() {
		System.out.println("Mgr - conductInterview");
	}
	
	/*
	public void attendTraining() {
		System.out.println("Mgr - attendTraining");
	}
	*/
	public void attendTraining(String[] trainingSchedule) {
		System.out.println("Mgr - attendTraining:");
		//System.out.println(trainingSchedule);
	}
}

class Programmer extends Employee implements Trainable
{
	String[] programmingLanguages;
	void writeCode() {}
	
	/*
	public void attendTraining() {
		System.out.println("Prog - attendTraining");
	}
	*/
	
	public void attendTraining(String[] trainingSchedule) {
		System.out.println("Prog - attendTraining:");
	}
}

public class Interface {
	public static void main(String args[])
	{
		Programmer p = new Programmer();
		Manager m = new Manager();
		
		String course[] = {"m","p"};
		
		p.attendTraining(course);
		
		m.attendTraining(course);
		m.conductInterview();
	}
}
