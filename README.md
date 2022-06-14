# Directolor
ESP32 + NRF24L01+ library to control Levolor blinds


This is a library known to work with ESP32 and NRF24L01+.  It allows direct control of Levolor blinds by replaying stored remote codes.  The easiest way to use this is to install the library and open the Directolor example.  You can then clone one of the Directolor remotes to a remote you own.  Then you can program your shades to the new cloned remote or using Directolor and control the blinds with either Directolor or your own remote.  

This differs from Wevolor (https://wevolor.com/) in that you DON'T need a Levolor 6 button remote.  

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
