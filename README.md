# Directolor
ESP32 + NRF24L01+ library to control Levolor blinds


This is a library known to work with ESP32 and NRF24L01+.  It allows direct control of Levolor blinds by replaying stored remote codes.  The easiest way to use this is to install the library and open the Directolor example.  You can then clone one of the Directolor remotes to a remote you own.  Then you can program your shades to the new cloned remote or using Directolor and control the blinds with either Directolor or your own remote.  

This differs from Wevolor (https://wevolor.com/) in that you DON'T need a Levolor 6 button remote.  

Typical usage:

1.	download solution, install to your libraries folder
2.	open the GetBlindCodes example
3.	choose to either copy the Directolor remotes to your remotes or copy your remotes to Directolor

Copy Directolor remotes to your remotes:
1.	open the battery door on your remote
2.	using a paper clip or similar, put the remote into duplicate mode
3.	hold the all key on your current remote
4.	using the serial monitor, choose the correct remote, and send the join command
5.	reset your shades to the remote you just duplicated

Copy your remotes to Directolor:
1.	using the serial monitor, put Directolor into Remote Search Mode
2.	press the stop button on your current remote with a single channel selected
3.	using the serial monitor, dump the remote codes
4.	copy the remote codes into Directolor.h

Test that you can control your shades via the serial monitor (open, close, stop, etc)

Once it is working, open the Directolor example.
Update your WiFi params.
Connect to http://directolor (or the IP address - logged to serial monitor)
Test that you can control your shades via the web interface

Please report any issues here.

To connect the ESP32 to the NRF24L01+ connect:
<br>(Some have recommended a 10uF cap across ground and 3.3V - I haven't needed the cap.)
<table>
  <tr>
    <th>ESP32 pin</th>
    <th>rf24 pin</th>
  </tr>
  <tr>
    <td>ground</td>
    <td>1</td>
  </tr>
  <tr>
    <td>3.3v</td>
    <td>2</td>
  </tr>
  <tr>
    <td>22</td>
    <td>3 (CE)</td>
  </tr>
  <tr>
    <td>21</td>
    <td>4 (CSN)</td>
  </tr>
  <tr>
    <td>18</td>
    <td>5 (SCK)</td>
  </tr>
  <tr>
    <td>23</td>
    <td>6 (MOSI)</td>
  </tr>
    <tr>
    <td>19</td>
    <td>7 (MISO)</td>
  </tr>
</table>
