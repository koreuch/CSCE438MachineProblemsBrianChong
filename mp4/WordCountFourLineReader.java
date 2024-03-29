import java.io.IOException;
import java.util.StringTokenizer;

import org.apache.hadoop.mapreduce.lib.input.NLineInputFormat;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.mapreduce.JobContext;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.io.IntWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

import org.apache.hadoop.classification.InterfaceAudience;
import org.apache.hadoop.classification.InterfaceStability;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.Seekable;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.compress.CodecPool;
import org.apache.hadoop.io.compress.CompressionCodec;
import org.apache.hadoop.io.compress.SplitCompressionInputStream;
import org.apache.hadoop.io.compress.SplittableCompressionCodec;
import org.apache.hadoop.io.compress.CompressionCodecFactory;
import org.apache.hadoop.io.compress.Decompressor;
import org.apache.hadoop.mapreduce.InputSplit;
import org.apache.hadoop.mapreduce.MRJobConfig;
import org.apache.hadoop.mapreduce.RecordReader;
import org.apache.hadoop.mapreduce.TaskAttemptContext;

import org.apache.hadoop.mapreduce.lib.input.LineRecordReader;

import org.apache.hadoop.mapreduce.lib.input.TextInputFormat;

import java.util.Arrays;

public class WordCountFourLineReader {//


public static class fourLineReader extends LineRecordReader{

    public fourLineReader(){
      super();
      value = new Text();
    }

    
    private Text value; // i need my own value that isn't in the super function
    
    public Text getCurrentValue(){
        return value;
    }

    public boolean nextKeyValue() throws IOException {
        // do this four times
        String fourLines = new String("");
        int linesRead = 0;
        while (linesRead != 4){
          super.nextKeyValue(); // "\n"
          Text cv = super.getCurrentValue();
          if (cv == null){
            return false;
          }
          else{
            fourLines = (fourLines + cv.toString());
            
            linesRead = linesRead + 1; 
            
            if (linesRead < 4) {
              fourLines = fourLines + "\n";
            }
          }
        }// end while 
      value = new Text(fourLines);
      return true;
    }

  }
  


  public static class TokenizerMapper
       extends Mapper<Object, Text, Text, IntWritable>{

    private final static IntWritable one = new IntWritable(1);
    private Text word = new Text();

    public void map(Object key, Text value, Context context
                    ) throws IOException, InterruptedException {
      // int num = 0;
      int iteration = 0;
      String hour = "";
      String[] arr = (value.toString()).split("[\\s\n]+");
      boolean readT = false;
      // word.set(value.toString());
      // context.write(word,one);
      for (int i = 0; i < arr.length; i ++) {
        //   word.set(arr[i]);
        // context.write (word,one);
        // continue;
          if ("T".equals(arr[0]) && !readT) {
            String[] arr1 = arr[2].split(":");
            hour = new String(arr1[0]);
            readT = true;
            continue;
          }
          if (arr[i].contains("sleep")){
            if (!readT) {
              continue;
            }
            word.set(hour + ",");
            context.write(word, one);
            break;
          }
        
        
      }
    }
  }
// just making a test comment to see what happens in the container
  public static class IntSumReducer
       extends Reducer<Text,IntWritable,Text,IntWritable> {
    private IntWritable result = new IntWritable();

    public void reduce(Text key, Iterable<IntWritable> values,
                       Context context
                       ) throws IOException, InterruptedException {
      int sum = 0;
      for (IntWritable val : values) {
        sum += val.get();
      }
      result.set(sum);
      context.write(key, result);
    }
  }

public static class fourLineFormat extends TextInputFormat {
  public fourLineFormat() {
    
  }

  public RecordReader<LongWritable, Text> createRecordReader(InputSplit split, TaskAttemptContext context) {
    fourLineReader reader = new fourLineReader();
    try {
      reader.initialize(split, context);
    } catch (Exception e) {
      e.printStackTrace();
    }
    return reader;
  }
}



  

  public static void main(String[] args) throws Exception {
    Configuration conf = new Configuration();
    conf.set("mapreduce.framework.name", "yarn");
    Job job = Job.getInstance(conf);
    job.setInputFormatClass(fourLineFormat.class);
    job.setJarByClass(WordCountFourLineReader.class);
    job.setMapperClass(TokenizerMapper.class);
    job.setCombinerClass(IntSumReducer.class);
    job.setReducerClass(IntSumReducer.class);
    job.setOutputKeyClass(Text.class);
    job.setOutputValueClass(IntWritable.class);
    FileInputFormat.addInputPath(job, new Path(args[0]));
    FileOutputFormat.setOutputPath(job, new Path(args[1]));
    System.exit(job.waitForCompletion(true) ? 0 : 1);
  }
}