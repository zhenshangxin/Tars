// **********************************************************************
// This file was generated by a TARS parser!
// TARS version 1.0.1.
// **********************************************************************

package com.qq.tars.generated.tars;

import com.qq.tars.protocol.util.*;
import com.qq.tars.protocol.annotation.*;
import com.qq.tars.protocol.tars.*;
import com.qq.tars.protocol.tars.annotation.*;

@TarsStruct
public class PatchInfo {

	@TarsStructProperty(order = 0, isRequire = true)
	public boolean bPatching = false;
	@TarsStructProperty(order = 1, isRequire = true)
	public int iPercent = 0;
	@TarsStructProperty(order = 2, isRequire = true)
	public int iModifyTime = 0;
	@TarsStructProperty(order = 3, isRequire = true)
	public String sVersion = "";
	@TarsStructProperty(order = 4, isRequire = true)
	public String sResult = "";
	@TarsStructProperty(order = 5, isRequire = false)
	public boolean bSucc = false;

	public boolean getBPatching() {
		return bPatching;
	}

	public void setBPatching(boolean bPatching) {
		this.bPatching = bPatching;
	}

	public int getIPercent() {
		return iPercent;
	}

	public void setIPercent(int iPercent) {
		this.iPercent = iPercent;
	}

	public int getIModifyTime() {
		return iModifyTime;
	}

	public void setIModifyTime(int iModifyTime) {
		this.iModifyTime = iModifyTime;
	}

	public String getSVersion() {
		return sVersion;
	}

	public void setSVersion(String sVersion) {
		this.sVersion = sVersion;
	}

	public String getSResult() {
		return sResult;
	}

	public void setSResult(String sResult) {
		this.sResult = sResult;
	}

	public boolean getBSucc() {
		return bSucc;
	}

	public void setBSucc(boolean bSucc) {
		this.bSucc = bSucc;
	}

	public PatchInfo() {
	}

	public PatchInfo(boolean bPatching, int iPercent, int iModifyTime, String sVersion, String sResult, boolean bSucc) {
		this.bPatching = bPatching;
		this.iPercent = iPercent;
		this.iModifyTime = iModifyTime;
		this.sVersion = sVersion;
		this.sResult = sResult;
		this.bSucc = bSucc;
	}

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = 1;
		result = prime * result + TarsUtil.hashCode(bPatching);
		result = prime * result + TarsUtil.hashCode(iPercent);
		result = prime * result + TarsUtil.hashCode(iModifyTime);
		result = prime * result + TarsUtil.hashCode(sVersion);
		result = prime * result + TarsUtil.hashCode(sResult);
		result = prime * result + TarsUtil.hashCode(bSucc);
		return result;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof PatchInfo)) {
			return false;
		}
		PatchInfo other = (PatchInfo) obj;
		return (
			TarsUtil.equals(bPatching, other.bPatching) &&
			TarsUtil.equals(iPercent, other.iPercent) &&
			TarsUtil.equals(iModifyTime, other.iModifyTime) &&
			TarsUtil.equals(sVersion, other.sVersion) &&
			TarsUtil.equals(sResult, other.sResult) &&
			TarsUtil.equals(bSucc, other.bSucc) 
		);
	}

	public void writeTo(TarsOutputStream _os) {
		_os.write(bPatching, 0);
		_os.write(iPercent, 1);
		_os.write(iModifyTime, 2);
		if (null != sVersion) {
			_os.write(sVersion, 3);
		}
		if (null != sResult) {
			_os.write(sResult, 4);
		}
		_os.write(bSucc, 5);
	}

	public void readFrom(TarsInputStream _is) {
		this.bPatching = _is.read(bPatching, 0, true);
		this.iPercent = _is.read(iPercent, 1, true);
		this.iModifyTime = _is.read(iModifyTime, 2, true);
		this.sVersion = _is.read(sVersion, 3, true);
		this.sResult = _is.read(sResult, 4, true);
		this.bSucc = _is.read(bSucc, 5, false);
	}

}
