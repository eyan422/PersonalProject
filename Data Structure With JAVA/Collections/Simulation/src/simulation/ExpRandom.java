package simulation;

public class ExpRandom extends java.util.Random {
	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;
	private double mean;
	
	public ExpRandom(double mean) {
		this.mean = mean;
	}
	
	public double nextDouble() {
		return -mean*Math.log(1 - Math.random());
	}
	
	public int nextInt() {
		return (int)Math.ceil(nextDouble());
	}
}