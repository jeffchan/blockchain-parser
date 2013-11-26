
BITCOIN TIP JAR: "1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya"


Initial Release : July 1, 2013


Project Location: https://code.google.com/p/blockchain/


This is released under MIT license, so, go to town.


Please send bugfixes, improvements, feature requests to: jratcliffscarab@gmail.com


I have an important request for anyone reading this.  I need someone to write me a script
which can download the historical pricing data of bitcoin over time and write it out to
some text or binary file that I can parse.  I would like the average price of bitcoin, in
US dollars, for every single day since data was first available.  I wouldn't mind if the
script would let me dial in the precision of that, so I could get say every hour if I wanted.
That dataset is fairly tiny compared to the entire bitcoin blockchain anyway, so I'm not
worried about the size.


My goal is to write a tool which can evaluate the the stored 'value' of all bitcoins currently
in circulation; by recording the exchange rate when they were last traded relative to the
current price today.


There has been a lot of discussion/concern regarding the amount of dormant bitcoins out there,
and a tool like this would be useful for gaining insight into this.  It would be of particular
interest to be able to know exactly when long dormant and presumed orphan coins come out of
hiding and re-enter the market.


One key feature of my analysis will involve detecting bitcoins that were dormant for a very
long time and then, suddenly, were resurrected from the dead.  I'm hoping some patterns may
allow us to be able to statistically predict how many of the dormant bitcoins are truly dead/lost
forever versus how many might come back and disrupt the market in the future.


A phenomenal piece of research was published on this topic, but that data becomes almost
immediately obsolete as the markets change rapidly.  I'm very curious to know how many
presumed dead bitcoins came to life since they did their study.


--------------------------------------------------------------------


This is a minimal C++ code snippet to read the bitcoin block chain one by one into memory.


It is comprised of just two source files; a header file describing the bitcoin blockchain
data layout and an implementation CPP.  They are, combined, only a few hundred lines of
source and the bulk of both source files is really just comments.


The header file can be browsed here.  BlockChain.h

And the implementation here: BlockChain.cpp


The GoogleCode project includes a windows console application and a Microsoft Visual Studio 2008
solution and project file.  The code should easily compile on any platform so long as it's little-endian.


This sample contains no code which actually interprets the *meaning* of that data; it just gets
it loaded into a data structure suitable for further processing.   Most likely in any future
articles I publish on this topic you will see examples of how to extract the meaning of this
raw data into data structures suitable for analysis.


This code uses no memory allocation, no STL, no BOOST, no containers, and it is only a couple
of hundreds lines of C++.  It does use hard-coded arrays with fixed limits in their sizes.
This is because the actual data in the bitcoin blockchain does not ever exceed certain limits;
so to avoid a great deal of unnecessary memory allocation this parser uses fixed buffers instead.


It is also pretty fast though, I suppose, it could be made faster, but not by a lot.  Some people
might point out that it could be made faster if it cached the source block-chain file but, really,
virtually all operating systems do that under the hood any way so I doubt it would make much difference.
As it is, it reads one bitcoin blockchain 'block' into memory at a time, parses the transactions,
inputs, and outputs, and returns them to the caller.  A person who might use this code snippet would,
most likely, take this data and convert it into some other more reasonable format for further processing
and analysis.  At least that's what I plan to do.


On my machine the bitcoin blockchain data is comprised of 68 files each about 128mb apiece totaling 9.2gb.


My machine parses this entire data set in roughly 95 seconds.


It is important to note that this code assumes that you are running on a small-endian machine, like
an X86.  It *does not* run on big-endian machines (like a PowerPC for example).  If you have a big-endian
machine, what can I say, get a real processor.


This code snippet was written by John W. Ratcliff (jratcliffscarab@gmail.com) on June 30, 2013 on a
lazy rainy Sunday afternoon


I wrote this code snippet for two reasons.  First, I just wanted to understand the bitcoin blockchain
format myself and, since I run the full bitcoin-qt client on my machine, I have all that data stored
on my hard drive anyway.


The second reason I wrote it is that I have an interest in doing some analysis on the block-chain data
and, to do that, I first need to be able to parse the transactions.  When I looked at available resources
on the internet they all use various scripting languages and JSON-RPC.  That just screams stunningly,
outrageously, absurdly, slow to me.


The specific data I want to analyze is a study of the historical value of outstanding and current bitcoins
in circulation.


There was an excellent paper published on this, but it was produced by processing an insane collection
of HTML files to get the output which makes their data almost instantly obsolete.


Right now there is a big mystery about how many of the dormant bitcoins are irrevocably lost or are merely
waiting to be cashed in at some point in the future.  There is really no way to know for sure, however,
we can look at how many presumed dormant coins come out of hiding over time.  It's an interesting data
mining exercise at any rate, and I wanted to be able to play around with exploring the dataset.


Before I wrote this code snippet, like a good programmer, I first looked around the internet to see if I
could just download something that would do the same thing.


However, yet once again, I ran into the same nonsense I always run into.  The most commonly referenced C++
code sample that shows how to read the blockchain has enormous dependencies and does not build out of the box.
I find this sort of thing terribly annoying.


This code snippet is just a single header file and a single CPP.  In theory it should compile on any platform,
all you have to do is revise a couple of typedefs at the top of the header to declare the basic int sizes on
your platform. That's it.  It doesn't use the STL, Boost, or another heavyweight dependencies above and beyond
standard library type stuff.


I did find this excellent reference online; from which this code was written.  Think of this code snippet at
essentially just a reference implementation of what is already covered on Jame's blog.


 If you find this code snippet useful; you can tip me at this bitcoin address:


BITCOIN TIP JAR: "1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya"


http://james.lab6.com/2012/01/12/bitcoin-285-bytes-that-changed-the-world/


 https://en.bitcoin.it/wiki/Protocol_specification


 One problem with Jame's specification is that it's not always super clear what the hierarchy of the input
 data is; I hope that the classes in this header file make this a bit more clear.


An important note, a number of the inputs in the blockchain are marked as 'variable length integers'
(presumably to 'save space' even though they really don't) The variable length integer is capable of being
as large as 64 bits but, in actuality, never is. That's why all of the integers in the following data structures
are 32 bits in size instead; simply for the sake of simplicity.  This sample runs just fine compiled for 32 bit.


A couple of items; sometimes you can run out of blockchain data before you reach the end of the file.  What
I discovered is that past a certain point the file just contains zeroes.


This was not documented in Jame's page; but that it is what I encountered in the input data set.


There are also many cases where the actual data in the block is a lot less than the reported block-length.
I'm going to assume that this too is normal and expected.

