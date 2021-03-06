#include "misc.h"

#include <android/log.h>
#define LOGCAT(...) __android_log_print(ANDROID_LOG_DEBUG , "android-os-monitor", __VA_ARGS__)

processor_info cur_processor[8];
float cputemp = 0;
int cur_processor_num = 0;

void misc_dump_processor()
{
	char buf[128];
	int i;

	for(i=0; i<8; i++)
	{
		sprintf(buf, CPUINFO_MAX, i);

		// get cpu max freq
		FILE *cpufile = fopen(buf, "r");
		if(!cpufile)
			cur_processor[i].cpumax =0;
		else
		{
			fscanf(cpufile, "%d", &cur_processor[i].cpumax);
			fclose(cpufile);
			cur_processor_num = i+1;
		}

		sprintf(buf, CPUINFO_MIN, i);

		// get cpu min freq
		cpufile = fopen(buf, "r");
		if(!cpufile)
			cur_processor[i].cpumin =0;
		else
		{
			fscanf(cpufile, "%d", &cur_processor[i].cpumin);
			fclose(cpufile);
		}

		sprintf(buf, CPU_SCALING_CUR, i);

		// get scaling cur freq
		cpufile = fopen(buf, "r");
		if(!cpufile)
			cur_processor[i].scalcur =0;
		else
		{
			fscanf(cpufile, "%d", &cur_processor[i].scalcur);
			fclose(cpufile);
		}

		sprintf(buf, CPU_SCALING_MAX, i);

		// get scaling max freq
		cpufile = fopen(buf, "r");
		if(!cpufile)
			cur_processor[i].scalmax =0;
		else
		{
			fscanf(cpufile, "%d", &cur_processor[i].scalmax);
			fclose(cpufile);
		}

		sprintf(buf, CPU_SCALING_MIN, i);

		// get scaling min freq
		cpufile = fopen(buf, "r");
		if(!cpufile)
			cur_processor[i].scalmin =0;
		else
		{
			fscanf(cpufile, "%d", &cur_processor[i].scalmin);
			fclose(cpufile);
		}

		sprintf(buf, CPU_SCALING_GOR, i);

		// get scaling governor
		cpufile = fopen(buf, "r");
		if(!cpufile)
			strcpy(cur_processor[i].scalgov, "");
		else
		{
			fscanf(cpufile, "%s", cur_processor[i].scalgov);
			fclose(cpufile);
		}

	}

	// OMAP3430 temperature
	FILE *cpufile = fopen(OMAP_TEMPERATURE, "r");
	cputemp = 0.f;
	if(cpufile)
	{
		unsigned int omaptemp = 0;
		fscanf(cpufile, "%d", &omaptemp);
		fclose(cpufile);
		cputemp = omaptemp;
	}

	// Tegra 3 SMBus temperature
	cputemp = misc_tegra3_get_processor_temperature();

	// Tegra 3 diode temperature
	if(cputemp == 0.0f)
	{
		cpufile = fopen(TEGRA3_TEMPERATURE, "r");
		if (cpufile)
		{
			unsigned int rawtemp = 0;
			fscanf(cpufile, "%d", &rawtemp);
			fclose(cpufile);
			cputemp = rawtemp / 1000.;
		}
	}
}

int misc_get_processor_cpumax(int num)
{
	return cur_processor[num].cpumax;
}

int misc_get_processor_cpumin(int num)
{
	return cur_processor[num].cpumin;
}

int misc_get_processor_scalcur(int num)
{
	return cur_processor[num].scalcur;
}

int misc_get_processor_scalmax(int num)
{
	return cur_processor[num].scalmax;
}

int misc_get_processor_scalmin(int num)
{
	return cur_processor[num].scalmin;
}

void misc_get_processor_scalgov(int num, char* buf)
{
	strcpy(buf, cur_processor[num].scalgov);
	return;
}

int misc_get_processor_number()
{
	return cur_processor_num;
}

float misc_get_processor_cputemp()
{
	return cputemp;
}

int misc_tegra3_is_tegra3()
{
	FILE *cpufile = fopen(TEGRA3_TEMPERATURE, "r");
	if(cpufile)
	{
		fclose(cpufile);
		return 1;
	}
	return 0;
}

int misc_tegra3_get_enabled_core_count()
{
	int result = 0;
	FILE *cpufile = fopen(CPU_TEGRA3_MAX_CORES, "r");
	if(cpufile)
	{
		fscanf(cpufile, "%d", &result);
		fclose(cpufile);
	}

	return result;
}

int misc_tegra3_is_lowpower_group_active()
{
	char result[BUFFERSIZE] = { '\0' };	
	
	return misc_tegra3_get_active_cpu_group(result) && strcmp(result, "LP") == 0;
}

int misc_tegra3_get_active_cpu_group(char* result)
{
	FILE *cpufile = fopen(CPU_TEGRA3_ACTIVE_GROUP, "r");
	if(cpufile)
	{
		fscanf(cpufile, "%s", result);
		fclose(cpufile);
		return 1;
	}

	return 0;
}

/* defines from drivers/misc/nct1008.c */
#define EXTENDED_RANGE_OFFSET	64U
#define STANDARD_RANGE_MAX		127U
#define EXTENDED_RANGE_MAX		(150U + EXTENDED_RANGE_OFFSET)
static inline char nct1008_value_to_temperature(_Bool extended, unsigned char value)
{
	return extended ? (char)(value - EXTENDED_RANGE_OFFSET) : (char)value;
}

float misc_tegra3_get_processor_temperature()
{
	FILE *cpufile = fopen(CPU_TEGRA3_SMBUS_TEMPERATURE, "r");
	int tempValueHigh = -1,
	    tempValueLow = -1;

	if (cpufile)
	{
		int reg, value, read;
		char buffer[256];
		while (!feof(cpufile))
		{
			/* Read the line from the file, stripping the Register name */
			char* valueStr = NULL;
			memset(buffer, 0, sizeof(buffer));
			fgets(buffer, sizeof(buffer), cpufile);
			if ((valueStr = strstr(buffer, "Addr ")) == NULL)
				continue;

			/* Then sscanf the remainder of the string */
			read = sscanf(valueStr, "Addr = %*i Reg %i Value %i", &reg, &value);
			if (read != 2)
				continue;

			if (reg == 0x0)
				tempValueLow = value;
			else if (reg == 0x1)
				tempValueHigh = value;
		}
		fclose(cpufile);
	}

	if (tempValueHigh != -1 && tempValueLow != -1)
	{
		/* Based off nct1008_polling_func in nct1008.c */
		float result;
		tempValueLow >>= 6;
		tempValueLow *= 25;
		tempValueHigh = nct1008_value_to_temperature(1 /* for endeavouru: /arch/arm/mach-tegra/board-endeavoru-sensors.c */,
			tempValueHigh);

		result = tempValueLow;
		while (result > 1.0f)
			result /= 10.0f;
		result += tempValueHigh;
		return result;
	}

	return 0.0f;
}

power_info cur_powerinfo;

void misc_dump_power()
{
	char buf[128];
	FILE *fp;

	memset(&cur_powerinfo, 0, sizeof(power_info));

	fp = fopen(BATTERY_STATUS_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        strncpy(cur_powerinfo.status, buf, 16);
        fclose(fp);
    }

    fp = fopen(BATTERY_HEALTH_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%s", cur_powerinfo.health);
        fclose(fp);
    }

    fp = fopen(BATTERY_CAPACITY_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%d", &cur_powerinfo.capacity);
        fclose(fp);
    }

    fp = fopen(BATTERY_VOLTAGE_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%d", &cur_powerinfo.voltage);
        fclose(fp);
    }

    fp = fopen(BATTERY_TEMPERATURE_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%d", &cur_powerinfo.temperature);
        fclose(fp);
    }

    fp = fopen(BATTERY_TECHNOLOGY_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%s", cur_powerinfo.technology);
        fclose(fp);
    }

    fp = fopen(AC_ONLINE_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%d", &cur_powerinfo.aconline);
        fclose(fp);
    }

    fp = fopen(USB_ONLINE_PATH, "r");
    if(fp != 0)
    {
    	fgets(buf, 128, fp);
        sscanf(buf, "%d", &cur_powerinfo.usbonline);
        fclose(fp);
    }

}

int misc_get_power_capacity()
{
	return cur_powerinfo.capacity;
}

int misc_get_power_voltage()
{
	return cur_powerinfo.voltage;
}

int misc_get_power_temperature()
{
	return cur_powerinfo.temperature;
}

int misc_get_power_aconline()
{
	return cur_powerinfo.aconline;
}

int misc_get_power_usbonline()
{
	return cur_powerinfo.usbonline;
}

void misc_get_power_health(char *buf)
{
	snprintf(buf, BUFFERSIZE, "%s", cur_powerinfo.health);
	return;
}

void misc_get_power_status(char *buf)
{
	snprintf(buf, BUFFERSIZE, "%s", cur_powerinfo.status);
	return;
}

void misc_get_power_technology(char *buf)
{
	snprintf(buf, BUFFERSIZE, "%s", cur_powerinfo.technology);
	return;
}

/* File System Module */
struct statfs datafs;
struct statfs systemfs;
struct statfs sdcardfs;
struct statfs cachefs;


void misc_dump_filesystem()
{
	memset(&datafs, 0, sizeof(statfs));
	memset(&systemfs, 0, sizeof(statfs));
	memset(&sdcardfs, 0, sizeof(statfs));
	memset(&cachefs, 0, sizeof(statfs));

	statfs(DATA_PATH, &datafs);
	statfs(SYSTEM_PATH, &systemfs);
	statfs(SDCARD_PATH, &sdcardfs);
	statfs(CACHE_PATH, &cachefs);
}

double misc_get_filesystem_systemtotal()
{
	if(systemfs.f_blocks * systemfs.f_bsize == 0)
		return 0;
	return ((long long)systemfs.f_blocks * (long long)systemfs.f_bsize) / 1024;
}

double misc_get_filesystem_datatotal()
{
	if(datafs.f_blocks * datafs.f_bsize == 0)
		return 0;
	return ((long long)datafs.f_blocks * (long long)datafs.f_bsize) / 1024;
}

double misc_get_filesystem_sdcardtotal()
{
	if(sdcardfs.f_blocks * sdcardfs.f_bsize == 0)
		return 0;
	return ((long long)sdcardfs.f_blocks * (long long)sdcardfs.f_bsize) / 1024;
}

double misc_get_filesystem_cachetotal()
{
	if(cachefs.f_blocks * cachefs.f_bsize == 0)
		return 0;
	return ((long long)cachefs.f_blocks * (long long)cachefs.f_bsize) / 1024;
}

double misc_get_filesystem_systemused()
{
	if(systemfs.f_blocks * systemfs.f_bfree == 0)
		return 0;
	return ((long long)(systemfs.f_blocks - (long long)systemfs.f_bfree) * systemfs.f_bsize) / 1024;
}

double misc_get_filesystem_dataused()
{
	if(datafs.f_blocks * datafs.f_bfree == 0)
		return 0;
	return ((long long)(datafs.f_blocks - (long long)datafs.f_bfree) * datafs.f_bsize) / 1024;
}

double misc_get_filesystem_sdcardused()
{
	if(sdcardfs.f_blocks * sdcardfs.f_bfree == 0)
		return 0;
	return ((long long)(sdcardfs.f_blocks - (long long)sdcardfs.f_bfree) * sdcardfs.f_bsize) / 1024;
}

double misc_get_filesystem_cacheused()
{
	if(cachefs.f_blocks * cachefs.f_bfree == 0)
		return 0;
	return ((long long)(cachefs.f_blocks - (long long)cachefs.f_bfree) * cachefs.f_bsize) / 1024;
}


double misc_get_filesystem_systemavail()
{
	if(systemfs.f_bfree * systemfs.f_bsize == 0)
		return 0;
	return ((long long)systemfs.f_bfree * (long long)systemfs.f_bsize) / 1024;
}

double misc_get_filesystem_dataavail()
{
	if(datafs.f_bfree * datafs.f_bsize == 0)
		return 0;
	return ((long long)datafs.f_bfree * (long long)datafs.f_bsize) / 1024;
}

double misc_get_filesystem_sdcardavail()
{
	if(sdcardfs.f_bfree * sdcardfs.f_bsize == 0)
		return 0;
	return ((long long)sdcardfs.f_bfree * (long long)sdcardfs.f_bsize) / 1024;
}

double misc_get_filesystem_cacheavail()
{
	if(cachefs.f_bfree * cachefs.f_bsize == 0)
		return 0;
	return ((long long)cachefs.f_bfree * (long long)cachefs.f_bsize) / 1024;
}
