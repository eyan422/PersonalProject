package phone;

class Phone {
	private boolean tested;
	
	double weight;
	
	public void setTested(boolean val) {
		tested = val;
	}
	public boolean isTested() {
		return tested;
	}
	
	static public void main(String args[]){
		//return;
		
		Phone p = new Phone();
		p.setTested(true);
		System.out.println(p.isTested());		
	}
	
	public boolean sendSMS( String phNum, String msg) {
		boolean msgSentStatus = false;
		
		//if (send (phNum, msg))
		{
			msgSentStatus = true;
		}
		
		return msgSentStatus;
	}
	//. . rest of code of class Phone
	
	void setWeight(double val) {
		weight =val;
	}
	
	double getWeight() {
		return weight;
	}
}
